#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H

#include "small/rlist.h"

#include "customer.h"

struct solution {
	struct rlist routes;
	struct rlist ejection_pool;
};

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
