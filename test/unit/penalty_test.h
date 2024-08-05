#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_TEST_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_TEST_H

#include "customer.h"
#include "route.h"

/**
 * Inheritance is only used in tests to reduce code size. In the main
 * implementation it is better not to use it because it is expensive
 * in terms of performance (additional dereferences).
 */
struct penalty_vtab {
    void
    (*penalty_init)(struct route *r);

    double
    (*penalty_get_penalty)(struct route *r);

    double
    (*penalty_get_insert_penalty)(struct customer *v, struct customer *w);

    double
    (*penalty_get_insert_delta)(struct customer *v, struct customer *w);

    double
    (*penalty_get_replace_penalty)(struct customer *v, struct customer *w);

    double
    (*penalty_get_replace_delta)(struct customer *v, struct customer *w);

    double
    (*penalty_get_eject_penalty)(struct customer *v);

    double
    (*penalty_get_eject_delta)(struct customer *v);

    double
    (*penalty_two_opt_penalty_delta)(struct customer *v, struct customer *w);

    double
    (*penalty_out_relocate_penalty_delta)(struct customer *v, struct customer *w);

    double
    (*penalty_exchange_penalty_delta)(struct customer *v, struct customer *w);
};

static struct penalty_vtab vtab;

#define assert_eq(lhs, rhs) if(lhs!=rhs)exit(1)
#define assert_near(lhs, rhs) if(fabs(lhs-rhs)>1e-5)exit(1)

#define RANDOM_MODIFICATIONS_BEFORE_INSERT			\
	double delta = vtab.penalty_get_insert_delta(v, w);	\
	double before = vtab.penalty_get_penalty(route)

#define RANDOM_MODIFICATIONS_AFTER_INSERT			\
	double after = vtab.penalty_get_penalty(route);		\
	assert_near(after - before, delta)

#define RANDOM_MODIFICATIONS_BEFORE_EJECT			\
	double delta = vtab.penalty_get_eject_delta(v);		\
	double before = vtab.penalty_get_penalty(route)

#define RANDOM_MODIFICATIONS_AFTER_EJECT			\
	double after = vtab.penalty_get_penalty(route);		\
	assert_near(after - before, delta)

#define RANDOM_MODIFICATIONS_BEFORE_REPLACE			\
	double delta = vtab.penalty_get_replace_delta(v, w);	\
	double before = vtab.penalty_get_penalty(route)

#define RANDOM_MODIFICATIONS_AFTER_REPLACE			\
	double after = vtab.penalty_get_penalty(route);		\
	assert_near(after - before, delta)

#define RANDOM_MODIFICATIONS_BEFORE_TWO_OPT			\
	double delta = vtab.penalty_two_opt_penalty_delta(v, w);\
	double before = vtab.penalty_get_penalty(v_route)	\
			+ vtab.penalty_get_penalty(w_route)

#define RANDOM_MODIFICATIONS_AFTER_TWO_OPT			\
	double after = vtab.penalty_get_penalty(v_route)	\
		       + vtab.penalty_get_penalty(w_route);	\
	assert_near(after - before, delta)

#define RANDOM_MODIFICATIONS_BEFORE_OUT_RELOCATE			\
	double delta = vtab.penalty_out_relocate_penalty_delta(v, w);	\
	double before = vtab.penalty_get_penalty(v_route)		\
			+ vtab.penalty_get_penalty(w_route)

#define RANDOM_MODIFICATIONS_AFTER_OUT_RELOCATE			\
	double after = vtab.penalty_get_penalty(v_route)	\
		       + vtab.penalty_get_penalty(w_route);	\
	assert_near(after - before, delta)

#define RANDOM_MODIFICATIONS_BEFORE_EXCHANGE				\
	double delta = vtab.penalty_exchange_penalty_delta(v, w);	\
	double before = vtab.penalty_get_penalty(v_route)		\
			+ vtab.penalty_get_penalty(w_route)

#define RANDOM_MODIFICATIONS_AFTER_EXCHANGE			\
	double after = vtab.penalty_get_penalty(v_route)	\
		       + vtab.penalty_get_penalty(w_route);	\
	assert_near(after - before, delta)

#include "random_modifications.h"

#undef RANDOM_MODIFICATIONS_BEFORE_INSERT
#undef RANDOM_MODIFICATIONS_AFTER_INSERT
#undef RANDOM_MODIFICATIONS_BEFORE_EJECT
#undef RANDOM_MODIFICATIONS_AFTER_EJECT
#undef RANDOM_MODIFICATIONS_BEFORE_REPLACE
#undef RANDOM_MODIFICATIONS_AFTER_REPLACE
#undef RANDOM_MODIFICATIONS_BEFORE_TWO_OPT
#undef RANDOM_MODIFICATIONS_AFTER_TWO_OPT
#undef RANDOM_MODIFICATIONS_BEFORE_OUT_RELOCATE
#undef RANDOM_MODIFICATIONS_AFTER_OUT_RELOCATE
#undef RANDOM_MODIFICATIONS_BEFORE_EXCHANGE
#undef RANDOM_MODIFICATIONS_AFTER_EXCHANGE

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_TEST_H
