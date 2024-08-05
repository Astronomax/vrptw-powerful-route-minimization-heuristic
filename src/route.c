#include "route.h"

struct route *
route_new()
{
	struct route *r = malloc(sizeof(*r)); //TODO: use mempool
	rlist_create(&r->list);
	rlist_create(&r->in_routes);
	return r;
}

void
route_init(struct route *r, struct customer **arr, int n)
{
	struct customer *c;
	c = customer_dup(p.depot);
	rlist_add_tail_entry(&r->list, c, in_route);
	c->route = r;
	for(int j = 0; j < n; j++) {
		c = arr[j];
		rlist_move_tail_entry(&r->list, c, in_route);
		c->route = r;
	}
	c = customer_dup(p.depot);
	rlist_add_tail_entry(&r->list, c, in_route);
	c->route = r;
	route_init_penalty(r);
	route_check(r);
}

struct route *
route_dup(struct route *r)
{
	struct route *dup = malloc(sizeof(*dup));
	rlist_create(&dup->list);
	rlist_create(&dup->in_routes);
	struct customer *c;
	/** One mempool allocation for each customer in list */
	rlist_foreach_entry(c, &r->list, in_route) {
		struct customer *c_dup = customer_dup(c);
		rlist_add_tail_entry(&dup->list, c_dup, in_route);
		c_dup->route = dup;
	}
	assert(rlist_first_entry(&dup->list, struct customer, in_route)->id == 0);
	assert(rlist_last_entry(&dup->list, struct customer, in_route)->id == 0);
	return dup;
}

void
route_del(struct route *r)
{
	struct customer *c, *tmp;
	rlist_foreach_entry_safe(c, &r->list, in_route, tmp) {
		rlist_del_entry(c, in_route);
		customer_del(c);
	}
}

struct customer *
route_find_customer_by_id(struct route *r, int id)
{
	/** depot id */
	if (id == 0)
		return NULL;
	struct customer *c;
	rlist_foreach_entry(c, &r->list, in_route)
		if (id == c->id)
			return c;
	return NULL;
}