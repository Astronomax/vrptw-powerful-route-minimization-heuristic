#include "solution.h"

#include "dist.h"
#include "modification.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/fiber.h"
#include "core/random.h"
#include "core/exception.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

struct solution_meta {
    customer *idx[0];
};

bool solution_global_initialized = false;

/**
 * Arrays of neighbours sorted by dist from particular customer.
 * There are no depot in this arrays. Depots are processed separately.
 */
int neighbours_sorted[MAX_N_CUSTOMERS][MAX_N_CUSTOMERS];

void
init_neighbours_sorted()
{
	std::vector<customer *> cs(p.n_customers + 1);
	cs[0] = p.depot;
	{
		customer *c;
		rlist_foreach_entry(c, &p.customers, in_route) cs[c->id] = c;
	}
	assert(cs.begin() != cs.end());

#define init_row() {				\
std::sort(cs.begin() + 1, cs.end(),		\
        [c](customer *a, customer *b) {		\
    	return dist(c, a) < dist(c, b); });	\
std::transform(cs.begin() + 1, cs.end(),	\
       std::begin(neighbours_sorted[c->id]),	\
       [](customer *c) { return c->id; });	\
} while(0)
	struct customer *c = p.depot;
	init_row();
	rlist_foreach_entry(c, &p.customers, in_route)
		init_row();
#undef init_row
}

struct modification_neighbourhood_args {
	solution *s;
	route *r;
	int n_near;
	modification *m;
};

/**
 * pre-calculated data in order to quickly process intra-route
 * out-relocate modifications
 */
struct intra_route_out_relocate_data {
	route *r;
	customer *w;
	customer *id_to_customer[MAX_N_CUSTOMERS];
	double tw_penalty_delta;
};

struct modification_neighbourhood_data {
	modification_neighbourhood_args args;
	intra_route_out_relocate_data out_relocate_current;
	/** number of customers in route (excluding depots) */
	int n;
	/** random permutation of route customers (excluding depots) */
	customer *permutation[MAX_N_CUSTOMERS];
};

void
intra_route_out_relocate_init_delta(
	modification_neighbourhood_data *data,
	customer *v)
{
	intra_route_out_relocate_data *d =
		&data->out_relocate_current;
	v = d->id_to_customer[v->id];
	assert(modification_applicable(
		modification_new(INSERT, v, d->w)));
	data->args.m->delta_initialized = true;
	data->args.m->tw_penalty_delta = d->tw_penalty_delta +
					 tw_penalty_get_insert_delta(v, d->w);
	data->args.m->c_penalty_delta = 0.;
}

/** out-relocate to the tail of some route */
/*void
out_relocation_to_depot_tails(
	modification_neighbourhood_data *data,
	customer *w)
{
	assert(w->id != 0);
	args *a = &data->_args;
	solution *s = a->s;
	route *v_route = s->routes[0];
	for (int i = 0; i < s->n_routes; v_route = s->routes[++i]) {
		customer *v = depot_tail(v_route);
		*a->m = modification_new(OUT_RELOCATE, v, w);
		assert(modification_applicable(*a->m));
		if (w->route == v->route)
			intra_route_out_relocate_init_delta(data, v);
		fiber_yield();
	}
}*/

void
inter_route_modifications(
	modification_neighbourhood_data *data,
	customer *v, customer *w)
{
	assert(v->route != w->route);
	modification_neighbourhood_args *a = &data->args;

#define yield_modification(_type) do {					\
	*a->m = modification_new((_type), v, w);			\
	assert(modification_applicable(*a->m));			\
	fiber_yield();							\
	if (fiber_is_cancelled())					\
		return;							\
} while (0)

	/*
	 * Most inter-route candidates are generated with depot sentinels.
	 * Avoid constructing modifications that modification_applicable()
	 * would reject immediately.
	 */
	if (w->id == 0) {
		if (w == depot_tail(w->route))
			return;
		if (v->id != 0)
			yield_modification(TWO_OPT);
		return;
	}

	if (v->id == 0) {
		yield_modification(OUT_RELOCATE);
		return;
	}

	yield_modification(TWO_OPT);
	yield_modification(OUT_RELOCATE);
	yield_modification(EXCHANGE);

#undef yield_modification
}

void
intra_route_modifications(
	modification_neighbourhood_data *data,
	customer *v, customer *w)
{
	assert(v->route == w->route);
	assert(w->id == data->out_relocate_current.w->id);
	if (v->id == 0 || w->id == 0)
		return;
	modification_neighbourhood_args *a = &data->args;
	/** out-relocate */
	*a->m = modification_new(OUT_RELOCATE, v, w);
	if (modification_applicable(*a->m)) {
		intra_route_out_relocate_init_delta(data, v);
		fiber_yield();
		if (fiber_is_cancelled())
			return;
	}
	/** exchange */
	bool exact;
	*a->m = modification_new(EXCHANGE, v, w);
	if (modification_applicable(*a->m)) {
		a->m->tw_penalty_delta =
			tw_penalty_exchange_penalty_delta_lower_bound(v, w, &exact);
		if (exact) {
			a->m->delta_initialized = true;
			a->m->c_penalty_delta = 0.;
			fiber_yield();
			if (fiber_is_cancelled())
				return;
		}
	}
}

void
modification_neighbourhood_data_init(
	modification_neighbourhood_data *data,
	va_list ap)
{
	data->args.s = va_arg(ap, solution *);
	data->args.r = va_arg(ap, route *);
	data->args.n_near = va_arg(ap, int);
	data->args.m = va_arg(ap, modification *);

	data->out_relocate_current.w = nullptr;
	data->out_relocate_current.r = nullptr;
	/**
	 * To diversify the search a little, we consider the vertices
	 * of the route in random order.
	 */
	data->n = 0;
	customer *c;
	route_foreach(c, data->args.r)
		data->permutation[data->n++] = c;
	random_shuffle(data->permutation, data->n);
}

void
intra_route_out_relocate_data_create(
	intra_route_out_relocate_data *data,
	customer *w)
{
	assert(w->id != 0);
	/** WARNING: ALLOCATION! */
	data->r = route_dup(w->route);
	customer *v;
	route_foreach(v, data->r)
		data->id_to_customer[v->id] = v;
	data->w = data->id_to_customer[w->id];
	auto m = modification_new(EJECT, data->w, nullptr);
	assert(modification_applicable(m));
	data->tw_penalty_delta = tw_penalty_get_eject_delta(data->w);
	modification_apply(m);
}

void
intra_route_out_relocate_data_destroy(intra_route_out_relocate_data *data)
{
	if (data->r != nullptr) {
		route_delete(data->r);
		data->r = nullptr;
	}
	if (data->w != nullptr) {
		customer_delete(data->w);
		data->w = nullptr;
	}
}

void
modification_neighbourhood_data_destroy(modification_neighbourhood_data *data)
{
	intra_route_out_relocate_data_destroy(&data->out_relocate_current);
}

void
solution_global_init()
{
	if (unlikely(!solution_global_initialized)) {
		init_neighbours_sorted();
		solution_global_initialized = true;
	}
}

int
solution_modification_neighbourhood_f(va_list ap)
{
	if(!solution_global_initialized)
		solution_global_init();

	modification_neighbourhood_data *data =
		xregion_alloc_object(&fiber()->gc, typeof(*data));
	data->out_relocate_current.r = nullptr;

	modification_neighbourhood_data_init(data, ap);
	struct solution *s = data->args.s;
	solution_check_routes(s);
	struct customer **idx = s->meta->idx;

#define check_modifications() do { \
        /**
	 * a few simple checks to filter out obviously
	 * inapplicable modifications
	 */							\
        if (v != w && !is_ejected(v)) {				\
        	if (v->route != w->route)			\
        	        inter_route_modifications(data, v, w);	\
        	else if (w->id != 0)				\
        	        intra_route_modifications(data, v, w);	\
	}							\
	if (fiber_is_cancelled())				\
                goto finish;					\
} while(0)
	customer *v;
	for (int j = 0; j < data->n; j++) {
		customer *w = data->permutation[j];
		if (w->id != 0) {
			/** prepare for intra-route out-relocate */
			intra_route_out_relocate_data_destroy(
				&data->out_relocate_current);
			intra_route_out_relocate_data_create(
				&data->out_relocate_current, w);
			/**
			 * here we could only consider out-relocate, but to
			 * simplify the code we check all types of
			 * modifications, it does not cost too much
			 */
			for (int i = 0; i < s->n_routes; i++) {
				v = depot_tail(s->routes[i]);
				check_modifications();
			}
		}
		for (int i = 0; i < MIN(data->args.n_near, p.n_customers); i++) {
			assert(neighbours_sorted[w->id][i] != 0);
			v = idx[neighbours_sorted[w->id][i]];
			check_modifications();
		}
	}

finish:
	modification_neighbourhood_data_destroy(data);
	region_truncate(&fiber()->gc, 0);
	return 0;
}

solution_meta *
solution_meta_new(rlist *problem_customers)
{
	auto *meta = (solution_meta*)
		xmalloc(sizeof(customer *) * (p.n_customers + 1));
	customer *c, *tmp;
	rlist_foreach_entry_safe(c, problem_customers, in_route, tmp) {
		rlist_del_entry(c, in_route);
		meta->idx[c->id] = c;
	}
	return meta;
}

void
solution_meta_delete(solution_meta *meta)
{
	free(meta);
}

void
solution_print_debug(solution *s)
{
	printf("s->w: %p\n", s->w);
	printf("s->ejection_pool: ");
	struct customer *c;
	rlist_foreach_entry(c, &s->ejection_pool, in_eject)
		printf("%p, ", c);
	printf("\n");
	printf("s->n_routes: %d\n", s->n_routes);
	for (int i = 0; i < s->n_routes; i++) {
		route_foreach(c, s->routes[i])
			printf("%p, ", c);
		printf("\n");
	}
	fflush(stdout);
}

void
solution_print(solution *s)
{
	for (int i = 0; i < s->n_routes; i++) {
		customer *c;
		route_foreach(c, s->routes[i])
			printf("%d ", c->id);
		printf("\n");
	}
	fflush(stdout);
}

double
solution_routing_cost(solution *s)
{
	double total = 0.;
	for (int i = 0; i < s->n_routes; i++) {
		route *r = s->routes[i];
		customer *prev = depot_head(r);
		for (customer *curr = route_next(prev);
		     curr != depot_tail(r);
		     curr = route_next(curr)) {
			total += dist(prev, curr);
			prev = curr;
		}
		total += dist(prev, depot_tail(r));
	}
	return total;
}

void
solution_print_incumbent_json(solution *s, long elapsed_ms)
{
	printf(
		"incumbent_solution_json: "
		"{\"elapsed_ms\":%ld,\"num_routes\":%d,\"native_cost\":%.12f,\"routes\":[",
		elapsed_ms,
		s->n_routes,
		solution_routing_cost(s)
	);
	for (int i = 0; i < s->n_routes; i++) {
		if (i > 0)
			printf(",");
		printf("[");
		bool first_customer = true;
		customer *c;
		route_foreach(c, s->routes[i]) {
			if (c->id == 0)
				continue;
			if (!first_customer)
				printf(",");
			printf("%d", c->id);
			first_customer = false;
		}
		printf("]");
	}
	printf("]}\n");
	fflush(stdout);
}

solution *
solution_default(void)
{
	auto *s = (solution *)xmalloc(sizeof(solution) +
		sizeof(struct route *) * p.n_customers);

	s->w = nullptr;

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);
	s->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	rlist_create(&s->ejection_pool);
	s->n_routes = p.n_customers;
	int i = 0;
	customer *c;
	rlist_foreach_entry(c, &p.customers, in_route) {
		route *r = route_new();
		route_init(r, &s->meta->idx[c->id], 1);
		s->routes[i] = r;
		++i;
	}
	assert(i == p.n_customers);
	return s;
}

solution *
solution_decode(const char *file)
{
	std::string file_string{file};
	std::ifstream f{file_string};
	if (!f.is_open())
		panic("solution_decode: cannot open file '%s'", file);

	/* Parse routes: one route per line, space-separated 1-indexed customer IDs */
	std::vector<std::vector<int>> parsed_routes;
	std::string line;
	while (std::getline(f, line)) {
		/* skip blank lines and comments */
		size_t first = line.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			continue;
		if (line[first] == '#')
			continue;

		std::istringstream iss(line);
		std::vector<int> route_ids;
		int id;
		while (iss >> id)
			route_ids.push_back(id);
		if (route_ids.empty())
			continue;
		parsed_routes.push_back(std::move(route_ids));
	}

	if (parsed_routes.empty())
		panic("solution_decode: file '%s' contains no routes", file);

	/* Validate: all customers present exactly once, no depot, in range */
	bool used[MAX_N_CUSTOMERS + 1];
	memset(used, 0, sizeof(used));
	int total_customers = 0;

	for (int ri = 0; ri < (int)parsed_routes.size(); ri++) {
		if (parsed_routes[ri].empty())
			panic("solution_decode: route %d is empty", ri + 1);
		for (int cid : parsed_routes[ri]) {
			if (cid == 0)
				panic("solution_decode: depot (0) must not appear in initial solution");
			if (cid < 1 || cid > p.n_customers)
				panic("solution_decode: customer ID %d out of range [1, %d]",
				      cid, p.n_customers);
			if (used[cid])
				panic("solution_decode: customer %d appears more than once", cid);
			used[cid] = true;
			total_customers++;
		}
	}

	if (total_customers != p.n_customers) {
		int missing = 0;
		for (int i = 1; i <= p.n_customers; i++)
			if (!used[i]) missing++;
		panic("solution_decode: %d customer(s) missing from initial solution", missing);
	}

	/* Build solution following the same pattern as solution_default() */
	auto *s = (solution *)xmalloc(sizeof(solution) +
		sizeof(struct route *) * p.n_customers);

	s->w = nullptr;

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);
	s->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	rlist_create(&s->ejection_pool);
	s->n_routes = (int)parsed_routes.size();

	/* Temp array for route_init */
	struct customer *arr[MAX_N_CUSTOMERS];

	for (int i = 0; i < s->n_routes; i++) {
		const auto &rids = parsed_routes[i];
		int n = (int)rids.size();
		for (int j = 0; j < n; j++)
			arr[j] = s->meta->idx[rids[j]];

		route *r = route_new();
		route_init(r, arr, n);
		s->routes[i] = r;
	}

	/* Post-build validation */
	solution_check_missed_customers(s);
	if (!solution_feasible(s))
		panic("solution_decode: imported solution is infeasible "
		      "(time windows or capacity violated)");

	return s;
}

/* TODO: deprecate */
solution *
solution_dup(solution *s)
{
	auto *dup = (solution *)xmalloc(sizeof(solution) +
				       sizeof(struct route *) * p.n_customers);

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);

	dup->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	dup->w = ((s->w != nullptr) ? dup->meta->idx[s->w->id] : nullptr);

	rlist_create(&dup->ejection_pool);
	customer *c;
	rlist_foreach_entry(c, &s->ejection_pool, in_eject) {
		assert(c->id != 0);
		rlist_add_tail_entry(&dup->ejection_pool, dup->meta->idx[c->id], in_eject);
	}
	dup->n_routes = s->n_routes;
	for (int i = 0; i < dup->n_routes; i++) {
		route *r = route_new();
		r->size = s->routes[i]->size;
		route_foreach(c, s->routes[i]) {
			auto c_dup = (c->id == 0) ? customer_dup(c) :
				dup->meta->idx[c->id];
			r->customers[c->idx] = c_dup;
		}
		route_refresh_metadata_from(r, 0);
		route_init_penalty(r);
		route_check(r);
		dup->routes[i] = r;
	}
	return dup;
}

void
solution_move(solution *dst, solution *src)
{
	solution_check_missed_customers(dst);
	SWAP(dst->w, src->w);
	SWAP(dst->meta, src->meta);
	rlist_swap(&dst->ejection_pool, &src->ejection_pool);
	for (int i = 0; i < MAX(dst->n_routes, src->n_routes); i++)
		SWAP(dst->routes[i], src->routes[i]);
	SWAP(dst->n_routes, src->n_routes);
	solution_check_missed_customers(src);
	solution_delete(src);
}

void
solution_delete(solution *s)
{
	solution_meta_delete(s->meta);
	free(s->w);
	struct customer *c, *tmp;
	rlist_foreach_entry_safe(c, &s->ejection_pool, in_eject, tmp)
		customer_delete(c);
	for (int i = 0; i < s->n_routes; i++)
		route_delete(s->routes[i]);
	free(s);
}

double
solution_penalty(struct solution *s, double alpha, double beta)
{
	double penalty = 0.;
	for(int i = 0; i < s->n_routes; i++)
		penalty += route_penalty(s->routes[i], alpha, beta);
	return penalty;
}

bool
solution_feasible(solution *s)
{
	for(int i = 0; i < s->n_routes; i++)
		if (!route_feasible(s->routes[i]))
			return false;
	return true;
}

/*
int
split_by_feasibility(solution *s)
{
	int infeasible = 0;
	for (int i = 0; i < s->n_routes; i++) {
		if (!route_feasible(s->routes[i])) {
			SWAP(s->routes[infeasible], s->routes[i]);
			++infeasible;
		}
	}
	return infeasible;
}
*/

struct modification
solution_find_feasible_insertion(struct solution *s, struct customer *w)
{
	assert(is_ejected(w));
	if (!solution_global_initialized)
		solution_global_init();
#define check_insertion() do {						\
	struct modification m = modification_new(INSERT, v, w);		\
	if (modification_applicable(m)) {				\
		double penalty = modification_delta(m, 1., 1.);		\
		if (penalty < EPS5) {					\
			++n_feasible_insertions;				\
			if (randint(1, n_feasible_insertions) == 1)	\
				selected = m;				\
		}							\
	}								\
} while (0)
	int n_feasible_insertions = 0;
	struct modification selected = modification_new(INSERT, nullptr, w);
	customer *v;
	for (int i = 0; i < p.n_customers; i++) {
		int id = neighbours_sorted[w->id][i];
		assert(id != 0);
		v = s->meta->idx[id];
		check_insertion();
	}
	for (int i = 0; i < s->n_routes; i++) {
		v = depot_tail(s->routes[i]);
		check_insertion();
	}
#undef check_insertion
	return selected;
}

struct modification
solution_find_optimal_insertion(struct solution *s, struct customer *w,
				double alpha, double beta)
{
	assert(is_ejected(w));
	struct customer *v;
	struct modification opt_modification =
		modification_new(INSERT, nullptr, nullptr);
	double opt_penalty = INFINITY;
	for(int i = 0; i < s->n_routes; i++) {
		for (int j = 1; j < s->routes[i]->size; j++) {
			v = s->routes[i]->customers[j];
			struct modification m = modification_new(INSERT, v, w);
			double penalty = modification_delta(m, alpha, beta);
			if (penalty < opt_penalty) {
				if (penalty < EPS5)
					return m;
				opt_modification = m;
				opt_penalty = penalty;
			}
		}
	}
	return opt_modification;
}

struct customer *
solution_find_customer_by_id(struct solution *s, int id)
{
	return s->meta->idx[id];
}

void
solution_eliminate_random_route(struct solution *s)
{
	int route_idx = randint(0, s->n_routes - 1);
	struct route *r = s->routes[route_idx];
	SWAP(s->routes[route_idx], s->routes[s->n_routes - 1]);
	--s->n_routes;
	for (int i = 1; i + 1 < r->size; i++) {
		struct customer *c = r->customers[i];
		c->route = nullptr;
		c->idx = -1;
		rlist_add_tail_entry(&s->ejection_pool, c, in_eject);
	}
	customer_delete(depot_head(r));
	customer_delete(depot_tail(r));
	r->size = 0;
	route_delete(r);
}
