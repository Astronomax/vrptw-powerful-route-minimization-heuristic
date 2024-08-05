#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H

#include "small/rlist.h"
#include "lib/include/rlist_persistent.h"

#include "customer.h"

struct solution {
	struct rlist routes;
	struct rlist ejection_pool;
	rlist_persistent_history history;
};

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
