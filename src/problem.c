#include "problem.h"

#include <math.h>

struct problem p;

static double
customer_distance(struct customer *lhs, struct customer *rhs)
{
	double dx = lhs->x - rhs->x;
	double dy = lhs->y - rhs->y;
	return sqrt(dx * dx + dy * dy);
}

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
	p.depot = NULL;
	p.n_customers = 0;
	rlist_create(&p.customers);
}

void
problem_init_distance_matrix(void)
{
	struct customer *customers[MAX_N_CUSTOMERS + 1] = {0};
	customers[0] = p.depot;

	struct customer *c;
	rlist_foreach_entry(c, &p.customers, in_route)
		customers[c->id] = c;

	for (int i = 0; i <= p.n_customers; i++) {
		assert(customers[i] != NULL);
		for (int j = i; j <= p.n_customers; j++) {
			assert(customers[j] != NULL);
			double distance = customer_distance(customers[i], customers[j]);
			p.distance_matrix[i][j] = distance;
			p.distance_matrix[j][i] = distance;
		}
	}
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
