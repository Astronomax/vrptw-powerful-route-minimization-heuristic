#include "route.h"

#include <string.h>

struct route *
route_new(void)
{
	/* TODO: use mempool to allocate routes */
	struct route *r = xmalloc(sizeof(struct route));
	r->size = 0;
	rlist_create(&r->in_routes);
	return r;
}

void
route_init(struct route *r, struct customer **arr, int n)
{
	r->size = n + 2;
	r->customers[0] = customer_dup(p.depot);
	for (int j = 0; j < n; j++)
		r->customers[j + 1] = arr[j];
	r->customers[r->size - 1] = customer_dup(p.depot);
	route_refresh_metadata_from(r, 0);
	route_init_penalty(r);
	route_check(r);
}

void
route_init_penalty(struct route *r)
{
	c_penalty_init(r);
	tw_penalty_init(r);
	/*
	 * We don't use distance penalties yet. Enable it only for
	 * `distance.test` unit test. Now let's simply disable this test.
	 */
	//distance_init(r);
}

void
route_update_penalty(struct route *r, struct customer *forward_start,
		     struct customer *backward_start)
{
	assert(r != NULL);
	if (forward_start != NULL)
		assert(forward_start->route == r);
	if (backward_start != NULL)
		assert(backward_start->route == r);
	if (forward_start == NULL || backward_start == NULL) {
		route_init_penalty(r);
		return;
	}

	c_penalty_update_forward(forward_start);
	c_penalty_update_backward(backward_start);
	tw_penalty_update_forward(forward_start);
	tw_penalty_update_backward(backward_start);
}

void
route_insert_customer(struct route *r, int idx, struct customer *c)
{
	assert(idx >= 0 && idx <= r->size);
	assert(r->size < MAX_N_CUSTOMERS + 2);
	if (idx < r->size) {
		memmove(&r->customers[idx + 1], &r->customers[idx],
			sizeof(r->customers[0]) * (r->size - idx));
	}
	r->customers[idx] = c;
	++r->size;
	route_refresh_metadata_from(r, idx);
}

struct customer *
route_remove_customer(struct route *r, int idx)
{
	assert(idx >= 0 && idx < r->size);
	struct customer *removed = r->customers[idx];
	if (idx + 1 < r->size) {
		memmove(&r->customers[idx], &r->customers[idx + 1],
			sizeof(r->customers[0]) * (r->size - idx - 1));
	}
	--r->size;
	route_refresh_metadata_from(r, idx);
	removed->route = NULL;
	removed->idx = -1;
	return removed;
}

/* TODO: deprecate */
struct route *
route_dup(struct route *r)
{
	struct route *dup = xmalloc(sizeof(*dup));
	dup->size = r->size;
	rlist_create(&dup->in_routes);
	for (int i = 0; i < r->size; i++)
		dup->customers[i] = customer_dup(r->customers[i]);
	route_refresh_metadata_from(dup, 0);
	route_check(r);
	return dup;
}

void
route_delete(struct route *r)
{
	for (int i = 0; i < r->size; i++)
		customer_delete(r->customers[i]);
	free(r);
}

struct customer *
route_find_customer_by_id(struct route *r, int id)
{
	/** depot id */
	if (id == 0)
		return NULL;
	for (int i = 1; i + 1 < r->size; i++) {
		struct customer *c = r->customers[i];
		if (id == c->id)
			return c;
	}
	return NULL;
}

bool
route_feasible(struct route *r)
{
	return (tw_penalty_get_penalty(r) < EPS5 && c_penalty_get_penalty(r) < EPS5);
}

double
route_penalty(struct route *r, double alpha, double beta)
{
	return alpha * c_penalty_get_penalty(r) +
	       beta * tw_penalty_get_penalty(r);
}

/*
int
route_len(struct route *r)
{
	int len = 0;
	struct customer *c;
	rlist_foreach_entry(c, &r->list, in_route)
		++len;
	return len - 2;
}
*/

struct modification
route_find_optimal_insertion(struct route *r, struct customer *w,
				double alpha, double beta)
{
	assert(is_ejected(w));
	struct modification opt_modification =
		modification_new(INSERT, NULL, NULL);
	double opt_penalty = INFINITY;
	for (int i = 1; i < r->size; i++) {
		struct customer *v = r->customers[i];
		struct modification m = modification_new(INSERT, v, w);
		double penalty = modification_delta(m, alpha, beta);
		if (penalty < opt_penalty) {
			if (penalty < EPS5)
				return m;
			opt_modification = m;
			opt_penalty = penalty;
		}
	}
	return opt_modification;
}
