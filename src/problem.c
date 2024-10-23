#include "problem.h"

struct problem p;

void
problem_customers_dup(struct rlist *list)
{
	struct customer *c;
	rlist_foreach_entry(c, &p.customers, in_route)
		rlist_add_tail_entry(list, customer_dup(c), in_route);
}

void
problem_destroy(void)
{
	customer_delete(p.depot);
	struct customer *c, *tmp;
	rlist_foreach_entry_safe(c, &p.customers, in_route, tmp)
		customer_delete(c);
}

int
problem_routes_straight_lower_bound(void)
{
	double sum = 0.;
	struct customer *c;
	rlist_foreach_entry(c, &p.customers, in_route)
		sum += c->demand;
	return (int)ceil(sum / p.vc);
}
