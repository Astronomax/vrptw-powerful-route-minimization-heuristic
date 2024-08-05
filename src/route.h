#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H

#include "small/rlist.h"

#include "customer.h"

struct route {
	/**
	 * List of customers. Contract:
	 * rlist_first_entry(&list, in_route) ==
	 * rlist_last_entry(&list, in_route) ==
	 * p->depot
	 */
	struct rlist list;
	struct rlist in_routes;
};

#define route_check(r) \
	assert(rlist_first_entry(&r->list, struct customer, in_route)->id == 0); \
	assert(rlist_last_entry(&r->list, struct customer, in_route)->id == 0)

#define route_init_penalty(r)	\
	c_penalty_init(r);	\
	tw_penalty_init(r);	\
	distance_init(r)

#define route_foreach(c, r) rlist_foreach_entry(c, &r->list, in_route)

#define depot_head(r) rlist_first_entry(&r->list, struct customer, in_route)
#define depot_tail(r) rlist_last_entry(&r->list, struct customer, in_route)

struct route *
route_new();

void
route_init(struct route *r, struct customer **arr, int n);

struct route *
route_dup(struct route *r);

void
route_del(struct route *r);

struct customer *
route_find_customer_by_id(struct route *r, int id);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_ROUTE_H
