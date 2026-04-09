#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H

#include "problem.h"

double ALWAYS_INLINE
dist(struct customer *lhs, struct customer *rhs)
{
	return p.distance_matrix[lhs->id][rhs->id];
}

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_DIST_H
