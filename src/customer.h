#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_CUSTOMER_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_CUSTOMER_H

#include "math.h"

#include "small/rlist.h"
#include "tw_penalty.h"
#include "c_penalty.h"
#include "distance.h"

struct customer {
	int id;
	double x;
	double y;
	double demand;
	double e;
	double l;
	double s;
	tw_penalty_attr;
	c_penalty_attr;
	distance_attr;
	struct route *route;
	struct rlist in_route;
};

struct customer *
customer_dup(struct customer *c);

void
customer_del(struct customer *c);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_CUSTOMER_H