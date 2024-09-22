#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H

#include "small/rlist.h"

#include "customer.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct solution {
	struct rlist routes;
	uint32_t n_routes;
	struct rlist ejection_pool;
};

void
solution_init();

int
route_modification_neighbourhood_f(va_list ap);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
