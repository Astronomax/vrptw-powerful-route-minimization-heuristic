#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_PROBLEM_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_PROBLEM_H

#include "customer.h"
#include "small/rlist.h"

#define MAX_N_CUSTOMERS 2000

struct problem {
	double vc;
	struct customer *depot;
	struct rlist customers;
	int n_customers;
};

extern struct problem p;

void global_problem_init(void);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_PROBLEM_H
