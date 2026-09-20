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
	int64_t *ps = va_arg(ap, int64_t *);
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
	/**
	 * Used only for pruning condition c. See the comment at the condition
	 * of do-while.
	 */
	bool feasible[MAX_N_CUSTOMERS + 2];
	bool incremented_last = false;
	/** Initialize `a_earliest`. Used only for pruning condition c. */
	struct customer *prev = depot_head(r);
	prev->a_earliest = prev->e;
	for (int i = 1; i < r->size; i++) {
		struct customer *next = r->customers[i];
		next->a_earliest = MAX(next->e, prev->a + prev->s + dist(prev, next));
		assert(next->a == MIN(next->a_earliest, next->l));
		prev = next;
	}
	/** Is the capacity contraint violated initially. */
	bool capacity_violated = total_demand > p.vc;
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
			if (p_sum < *p_best && *k < k_max) {
				incremented_last = false;
				goto incr_k;
			}
			goto incr_last;
		}
		do {
		/** backtrack */
			if (unlikely(*k == 1)) {
				*k = 0;
				return 0;
			}
			/** Return the last ejected customer back. */
			p_sum -= ps[e_last->id];
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
			ne_last = ne[ne_size - 1];
			s_first = s[s_size - 1];
		incr_last:
			incremented_last = true;
			ne[ne_size++] = e_last;
			p_sum -= ps[e_last->id];
			total_demand += e_last->demand;
			--(*k);
		#ifndef NDEBUG
			prev = ne_last;
		#endif
			ne_last = e_last;
		#ifndef NDEBUG
			DEBUG_ASSERT_NEAR(ne_last->a_earliest_temp, MAX(ne_last->e,
				prev->a_temp + prev->s + dist(prev, ne_last)));
			DEBUG_ASSERT_NEAR(ne_last->a_temp, MIN(ne_last->a_earliest_temp, ne_last->l));
		#endif
			/*
			 *  just got back ---+
			 *                   v
			 * ... [e] [ne] [ne] [ne] [s ...]
			 *     <-----+---->  ^
			 *           |       +---- let's account this guy
			 * feasible [k - 1] now accounts only
			 */
			if (*k > 0)
				feasible[(*k) - 1] &= ne_last->a_earliest <= ne_last->l;
		incr_k:
			assert(s_first == s[s_size - 1]);
			e[*k] = s_first;
			p_sum += ps[s_first->id];
			total_demand -= s_first->demand;
			e_last = s_first;
			s_first = s[--s_size - 1];
			++(*k);
			/*
			 * just ejected, there no `ne` after it for now
			 *      v
			 * ... [e] [s ...]
			 */
			feasible[(*k) - 1] = e_last->a_earliest <= e_last->l;
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
			 ne_last->l < ne_last->a_earliest_temp ||
			 /* c)
			  * ... [e] [ne] [ne] [e] [s ...]
			  *     <----------->
			  * There is no initially infeasible among previous [e] and consecutive
			  * [ne] after it.
			  * - If [e] were infeasible, it means we discarded it, that makes sense.
			  * - If one of the [ne] was infeasible, it means it has become feasible
			  * (otherwise condition b would have triggered), that makes sense.
			  * Alternatively, [e] need not be ejected, the segment remains feasible,
			  * and the `a_earliest` value remains unchanged.
			  * Although for k > 2, this is a rather dubious statement. TODO: think about it.
			  * In the article, condition c looks different, and I also don't
			  * understand why it should work. Moreover, as I see it, I have counterexamples.
			  * This nonsense with `feasible` array is my attempt to fix the correctness
			  * of this condition as I understand it.
			  */
			 ((*k) > 1 && incremented_last && feasible[(*k) - 2] &&
			  ne_last->a_earliest_temp == ne_last->a_earliest &&
			  !capacity_violated));
	}
	unreachable();
}
