#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H

#include "customer.h"
#include "modification.h"
#include "small/rlist.h"
#include "utils.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct route {
    struct customer *customers[MAX_N_CUSTOMERS + 2];
    int size;
    struct rlist in_routes;
    int in_infeasibles_idx;
};

#define route_foreach(c, r) \
	for (int _route_i = 0; \
	     _route_i < (r)->size && (((c) = (r)->customers[_route_i]) || 1); \
	     ++_route_i)

#define route_foreach_from(c, from) \
	for (int _route_i = (from)->idx; \
	     _route_i < (from)->route->size && \
	     (((c) = (from)->route->customers[_route_i]) || 1); \
	     ++_route_i)

#define depot_head(r) ((r)->customers[0])
#define depot_tail(r) ((r)->customers[(r)->size - 1])

#define route_prev(c) ((c)->route->customers[(c)->idx - 1])
#define route_next(c) ((c)->route->customers[(c)->idx + 1])

static ALWAYS_INLINE int
route_non_depot_size(struct route *r)
{
	return r->size - 2;
}

static ALWAYS_INLINE void
route_refresh_metadata_from(struct route *r, int start_idx)
{
	if (start_idx < 0)
		start_idx = 0;
	for (int i = start_idx; i < r->size; i++) {
		r->customers[i]->route = r;
		r->customers[i]->idx = i;
	}
}

static ALWAYS_INLINE void
route_check(struct route *r)
{
	(void)r;
#ifndef NDEBUG
	assert(r->size >= 2);
	assert(depot_head(r)->id == 0);
	assert(depot_tail(r)->id == 0);
	struct customer *c;
	route_foreach(c, r) {
		assert(c->route == r);
		assert(c->idx >= 0 && c->idx < r->size);
		assert(r->customers[c->idx] == c);
	}
#endif
}

struct route *
route_new(void);

void
route_init(struct route *r, struct customer **arr, int n);

void
route_init_penalty(struct route *r);

void
route_update_penalty(struct route *r, struct customer *forward_start,
		     struct customer *backward_start);

void
route_insert_customer(struct route *r, int idx, struct customer *c);

struct customer *
route_remove_customer(struct route *r, int idx);

struct route *
route_dup(struct route *r);

void
route_delete(struct route *r);

struct customer *
route_find_customer_by_id(struct route *r, int id);

bool
route_feasible(struct route *r);

double
route_penalty(struct route *r, double alpha, double beta);

//int
//route_len(struct route *r);

struct modification
route_find_optimal_insertion(struct route *r, struct customer *w,
			     double alpha, double beta);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H
