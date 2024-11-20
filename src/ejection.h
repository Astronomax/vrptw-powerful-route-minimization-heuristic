#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_INSERT_EJECT_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_INSERT_EJECT_H

#include <stdarg.h>
#include <stdbool.h>
#include <bits/stdint-intn.h>
#include "small/rlist.h"

/**
 * Based on article "A powerful route minimization heuristic for the vehicle
 * routing problem with time windows" Yuichi Nagata, Olli Bräysy
 * (3.3. Finding the best insertion–ejection combination)
 */

#define feasible_ejections_attr	\
	/**
	 * We don't want the customer::a calculated in tw_init_penalty to break
	 * while iterating through feasible ejections.
	 */				\
	double a_temp;			\
	double a_quote

struct ejections_iterator {
	struct route *r;
	const int64_t *ps;
	int k_max;
	int64_t p_best;
	bool has_next_value;
	struct rlist e;
	struct rlist ne;
	struct rlist s;
	struct customer *ne_last;
	double total_demand;
	int64_t p_sum;
	int k;
	struct customer *e_last;
	struct customer *s_first;
};

void
ejections_iterator_create(struct ejections_iterator *it,
	struct route *r, const int64_t *ps, int k_max, int64_t p_best);

/**
 * @brief Iterate over feasible ejections (a subsets of route customers such
 * that if they were ejected, this route would not violate the constraints, i.e.
 * tw_penalty and c_penalty equals to zero). Iteration occurs in lexicographic
 * order and only subsets of size no more than \a k_max are considered. In this
 * case, the next ejection is returned only if the sum of p in it is less than
 * the current minimum (p_best).
 *
 * @param r 		route
 * @param k_max		maximum subset size
 * @param e		ejection (must be initially passed empty)
 * @param p_best	current minimum sum of p
 */
bool
ejections_iterator_next(struct ejections_iterator *it);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_INSERT_EJECT_H
