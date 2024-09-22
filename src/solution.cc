#include "solution.h"

#include "dist.h"
#include "modification.h"
#include "random_utils.h"

#include <cassert>

#include "core/fiber.h"
#include "core/random.h"
#include "core/exception.h"

#include <algorithm>

bool solution_initialized = false;

/**
 * Arrays of neighbours sorted by dist from particular customer.
 * There are no depot in this arrays. Depots are processed separately.
 */
struct customer *neighbours_sorted[MAX_N_CUSTOMERS][MAX_N_CUSTOMERS];

void
init_neighbours_sorted()
{
	std::vector<struct customer *> cs(p.n_customers + 1);
	cs[0] = p.depot;
	{
		struct customer *c;
		rlist_foreach_entry(c, &p.customers, in_route) cs[c->id] = c;
	}
	assert(cs.begin() != cs.end());

	for (auto c: cs) {
		std::sort(cs.begin() + 1, cs.end(),
		      [c](struct customer *a, struct customer *b) {
			return dist(c, a) < dist(c, b); });
		std::copy(cs.begin() + 1, cs.end(),
		      std::begin(neighbours_sorted[c->id]));
	}
}

struct args {
	struct solution *s;
	struct route *r;
	int n_near;
	struct modification *m;
	double *tw_penalty_delta;
	double *c_penalty_delta;
};

/**
 * pre-calculated data in order to quickly process intra-route
 * out-relocate modifications.
 */
struct intra_route_out_relocate_data {
	struct route *r;
	struct customer *v;
	struct customer *id_to_customer[MAX_N_CUSTOMERS];
	double tw_penalty_delta;
};

struct modification_neighbourhood_data {
	struct args args;
	struct intra_route_out_relocate_data intra_route_out_relocate_data;
	/** number of customers in route (excluding depots) */
	int n;
	/** random permutation of route customers (excluding depots) */
	struct customer *permutation[MAX_N_CUSTOMERS];
};

void
intra_route_out_relocate_init_delta(
	struct modification_neighbourhood_data *data,
	struct customer *w)
{
	struct intra_route_out_relocate_data *d =
		&data->intra_route_out_relocate_data;
	w = d->id_to_customer[w->id];
	assert(modification_applicable({INSERT, d->v, w}) == 0);
	*data->args.tw_penalty_delta = d->tw_penalty_delta +
		tw_penalty_get_insert_delta(d->v, w);
	*data->args.c_penalty_delta = 0.;
}

/** out-relocate to the tail of some route */
void
out_relocation_to_depot_tails(
	struct modification_neighbourhood_data *data,
	struct customer *v)
{
	struct args *args = &data->args;
	struct route *w_route;
	rlist_foreach_entry(w_route, &args->s->routes, in_routes) {
		struct customer *w = depot_tail(w_route);
		*args->m = {OUT_RELOCATE, v, w};
		assert(modification_applicable(*args->m) == 0);
		if (v->route == w->route) {
			intra_route_out_relocate_init_delta(data, w);
		} else {
			*data->args.tw_penalty_delta =
				tw_penalty_out_relocate_penalty_delta(v, w);
			*data->args.c_penalty_delta =
				c_penalty_out_relocate_penalty_delta(v, w);
		}
		fiber_yield();
	}
}

void
inter_route_modifications(
	struct modification_neighbourhood_data *data,
	struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	struct args *a = &data->args;
	for (int t = 0; t < modification_max; t++) {
		*a->m = {(modification_type) t, v, w};
		*a->tw_penalty_delta =
			modification_delta(*a->m, 1., 0.);
		*a->c_penalty_delta =
			modification_delta(*a->m, 0., 1.);
		if (modification_applicable(*a->m) == 0)
			fiber_yield();
	}
}

void
intra_route_modifications(
	struct modification_neighbourhood_data *data,
	struct customer *v, struct customer *w)
{
	assert(v->route == w->route);
	struct args *a = &data->args;
	/** out-relocate */
	if (v->id != 0) {
		*a->m = {OUT_RELOCATE, v, w};
		assert(modification_applicable(*a->m) == 0);
		intra_route_out_relocate_init_delta(data, w);
		fiber_yield();
	}
	/** exchange */
	bool exact;
	*a->tw_penalty_delta =
		tw_penalty_exchange_penalty_delta_lower_bound(v, w, &exact);
	if (exact) {
		*a->m = {EXCHANGE, v, w};
		assert(modification_applicable({EXCHANGE, v, w}) == 0);
		*a->c_penalty_delta = 0.;
		fiber_yield();
	}
}

void modification_neighbourhood_data_init(
	struct modification_neighbourhood_data *data,
	va_list ap)
{
	data->args.s = va_arg(ap, struct solution *);
	data->args.r = va_arg(ap, struct route *);
	data->args.n_near = va_arg(ap, int);
	data->args.m = va_arg(ap, struct modification *);
	data->args.tw_penalty_delta = va_arg(ap, double *);
	data->args.c_penalty_delta = va_arg(ap, double *);

	/**
	 * To diversify the search a little, we consider the vertices
	 * of the route in random order.
	 */
	data->n = 0;
	struct customer *c;
	route_foreach(c, data->args.r)
		data->permutation[data->n++] = c;
	random_shuffle(data->permutation, data->n);
}

void
intra_route_out_relocate_data_init(
	struct modification_neighbourhood_data *data,
	struct customer *v)
{
	struct intra_route_out_relocate_data *d =
		&data->intra_route_out_relocate_data;
	if (d->r != nullptr)
		route_del(d->r);
	/** WARNING: ALLOCATION! */
	d->r = route_dup(v->route);
	struct customer *w;
	route_foreach(w, v->route)
		d->id_to_customer[w->id] = w;
	d->v = d->id_to_customer[v->id];
	struct modification m = {EJECT, d->v, nullptr};
	assert(modification_applicable(m) == 0);
	d->tw_penalty_delta = tw_penalty_get_eject_delta(d->v);
	modification_apply(m);
}

int
route_modification_neighbourhood_f(va_list ap)
{
	assert(solution_initialized);

	size_t size;
	struct modification_neighbourhood_data *data =
		region_alloc_object(&fiber()->gc, typeof(*data), &size);
	if (data == nullptr)
		tnt_raise(OutOfMemory, size, "region_alloc_object", "modification_neighbourhood_data");
	modification_neighbourhood_data_init(data, ap);

	for (int j = 0; j < data->n; j++) {
		struct customer *v = data->permutation[j];
		/** prepare for intra-route out-relocate */
		if (v->id != 0)
			intra_route_out_relocate_data_init(data, v);
		/** out-relocate to the tail of some route */
		out_relocation_to_depot_tails(data, v);
		for (int i = 0; i < data->args.n_near; i++) {
			struct customer *w = neighbours_sorted[v->id][i];
			if (v == w || is_ejected(w))
				continue;
			if (v->route != w->route)
				inter_route_modifications(data, v, w);
			else
				intra_route_modifications(data, v, w);
		}
	}
	return 0;
}

void
solution_init()
{
	if (!solution_initialized) {
		init_neighbours_sorted();
		solution_initialized = true;
	}
}
