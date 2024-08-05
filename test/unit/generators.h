#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_GENERATORS_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_GENERATORS_H

#include "customer.h"
#include "problem.h"
#include "route.h"

void
generate_random_problem(int max_n_customers);

struct customer *
generate_random_customer();

struct route *
generate_random_route(struct customer *depot, int max_customer_count);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_GENERATORS_H
