#include "ejection.h"

#include "core/fiber.h"

#include "dist.h"
#include "route.h"

#define DEBUG_ASSERT_NEAR(lhs, rhs) assert(fabs((lhs)-(rhs)) < 1e-5)

int
feasible_ejections_f(va_list ap)
{
	struct route *r = va_arg(ap, struct route *);
	int k_max = va_arg(ap, int);
	struct customer **e = va_arg(ap, struct customer **);
	int *k = va_arg(ap, int *);
	int64_t *p_best = va_arg(ap, int64_t *);

	if (k_max <= 0)
		return 0;

	*k = 0;
	/**
	 * The layout:
	 *         ne: [ne] [ne] [ne] (non ejected)
	 *         |                   e_last
	 *         +----+--------+     v
	 * ... [e] [ne] [ne] [e] [ne] [e] [s ...]
	 *     +-------------+--------+   ^
	 *     |                          s_first
	 *     e: [e] [e] [e] (ejected)
	 */
	/** A stack of non ejected customers. */
	struct customer *ne[MAX_N_CUSTOMERS + 2];
	int ne_size = 1;
	struct customer *ne_last = depot_head(r);
	ne[0] = ne_last;
	ne_last->a_temp = ne_last->a_earliest_temp = ne_last->e;
	/** A stack representing the suffix after the last ejected customer. */
	struct customer *s[MAX_N_CUSTOMERS + 2];
	int s_size = 0;
	/** Put customers into an array and reverse for more convinient indexing. */
	struct customer *tmp;
	route_foreach_from(tmp, route_next(ne_last))
		s[s_size++] = tmp;
	for (int i = 0; i < s_size / 2; i++)
		SWAP(s[i], s[s_size - 1 - i]);
	/** Top of the stack. */
	struct customer *s_first = s[s_size - 1];
	double total_demand = depot_tail(r)->demand_pf;
	/** The total sum of 'p' values of the ejected customers. */
	int64_t p_sum = 0;
	/** The last ejected customer. Will be initialized after incr_k */
	struct customer *e_last;
	/** There is no ejected customers now. Let's eject first. */
	goto incr_k;
	for(;;) {
		if (/** Is better than current optimum */
		    p_sum < *p_best &&
		    /** Doesn't violate time-window constraint */
		    s_first->a_earliest_temp <= s_first->l &&
		    /** See the article. */
		    s_first->a_temp <= s_first->z &&
		    /** There is no infeasibilities on suffix. */
		    s_first->tw_sf == 0. &&
		    /** Doesn't violate capacity constraint */
		    total_demand <= p.vc) {
			/** Update `p_best` and pass the current optimum "up". */
			*p_best = p_sum;
			fiber_yield();
			if (fiber_is_cancelled())
				return 0;
		}
		/**
		 * If the end of the route has not been reached (there is some non-depot
		 * customers).
		 */
		if (s_first->id != 0) {
			/* a)
			 * If `p_sum` >= `p_best`, there is no point in ejecting any further
			 * vertices. None of the deeper branches will update the current optimum.
			 */
			if (p_sum < *p_best && *k < k_max)
				goto incr_k;
			goto incr_last;
		}
		do {
		/** backtrack */
			if (unlikely(*k == 1)) {
				*k = 0;
				return 0;
			}
			/** Return the last ejected customer back. */
			p_sum -= e_last->p;
			total_demand += e_last->demand;
			--(*k);
			/*
			 *                       ne_to_return
			 *                 <---------------------->
			 * before: ... [e] [ne] [ne] [ne] [ne] [ne] [e_last] [s ...]
			 *                 + move all this customers to s    ^
			 *                 + --------------------------------+
			 *
			 * after:  ... [e] [s ...]
			 */
			int ne_to_return = e_last->idx;
			assert(*k > 0);
			e_last = e[*k - 1];
			ne_to_return -= e_last->idx + 1;
			assert(ne_to_return >= 0);
			ne_size -= ne_to_return;
			s_size += ne_to_return + 1;
		#ifndef NDEBUG
			ne_last = ne[ne_size - 1];
		#endif
			s_first = s[s_size - 1];
		incr_last:
			ne[ne_size++] = e_last;
			p_sum -= e_last->p;
			total_demand += e_last->demand;
			--(*k);
		#ifndef NDEBUG
			DEBUG_ASSERT_NEAR(e_last->a_earliest_temp, MAX(e_last->e,
				ne_last->a_temp + ne_last->s + dist(ne_last, e_last)));
			DEBUG_ASSERT_NEAR(e_last->a_temp, MIN(e_last->a_earliest_temp, e_last->l));
		#endif
			ne_last = e_last;
		incr_k:
			assert(s_first == s[s_size - 1]);
			e[*k] = s_first;
			p_sum += s_first->p;
			total_demand -= s_first->demand;
			e_last = s_first;
			s_first = s[--s_size - 1];
			++(*k);
		/** update */
			assert(ne_last == ne[ne_size - 1]);
			/*
			 * Update e_last, so that when it returns to `ne` after `incr_last`,
			 * it already has the current value. There’s no real point in that,
			 * it could have been updated right there.
			 */
			e_last->a_earliest_temp = MAX(e_last->e,
				ne_last->a_temp + ne_last->s + dist(ne_last, e_last));
			e_last->a_temp = MIN(e_last->a_earliest_temp, e_last->l);
			s_first->a_earliest_temp = MAX(s_first->e,
				ne_last->a_temp + ne_last->s + dist(ne_last, s_first));
			s_first->a_temp = MIN(s_first->a_earliest_temp, s_first->l);
		} while (
			/* b)
			 * `ne_last` is already violating the constraint, so there is no point
			 * in examining deeper branches - backtrack.
			 */
			 ne_last->l < ne_last->a_earliest_temp);
	}
	unreachable();
}
