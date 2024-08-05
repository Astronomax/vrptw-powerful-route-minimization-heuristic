#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_C_PENALTY_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_C_PENALTY_H

#define c_penalty_attr 	\
    double demand_pf;	\
    double demand_sf

#include "customer.h"
#include "problem.h"
#include "route.h"

void
c_penalty_init(struct route *r);

double
c_penalty_get_penalty(struct route *r);

double
c_penalty_get_insert_penalty(struct customer *v, struct customer *w);

double
c_penalty_get_insert_delta(struct customer *v, struct customer *w);

double
c_penalty_get_replace_penalty(struct customer *v, struct customer *w);

double
c_penalty_get_replace_delta(struct customer *v, struct customer *w);

double
c_penalty_get_eject_penalty(struct customer *v);

double
c_penalty_get_eject_delta(struct customer *v);

#ifdef C_PENALTY_TEST
#include "modification.h"

double
tw_penalty_modification_apply_straight_delta(struct modification m);
#endif

#ifdef C_PENALTY_TEST
double
c_penalty_one_opt_penalty(struct customer *v, struct customer *w);
#endif

double
c_penalty_two_opt_penalty_delta(struct customer *v, struct customer *w);

double
c_penalty_out_relocate_penalty_delta_slow(struct customer *v, struct customer *w);

double
c_penalty_out_relocate_penalty_delta_fast(struct customer *v, struct customer *w);

double
c_penalty_out_relocate_penalty_delta(struct customer *v, struct customer *w);

double
c_penalty_exchange_penalty_delta(struct customer *v, struct customer *w);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_C_PENALTY_H
