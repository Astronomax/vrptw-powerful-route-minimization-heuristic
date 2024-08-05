#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H

#include "math.h"

#include "customer.h"

double ALWAYS_INLINE
dist(struct customer *lhs, struct customer *rhs)
{
	return sqrt(pow(lhs->x - rhs->x, 2.) + pow(lhs->y - rhs->y, 2.));
}

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H
