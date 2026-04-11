#include "ejection.h"

#include "core/fiber.h"

#include "dist.h"
#include "route.h"

#define DEBUG_ASSERT_NEAR(lhs, rhs) assert(fabs((lhs)-(rhs)) < 1e-5)

void init_a_earliest(struct route *r) {
	struct customer *next;
	struct customer *prev = depot_head(r);
	prev->a_earliest = prev->e;
	for (next = rlist_next_entry(prev, in_route);
	     !rlist_entry_is_head(next, &r->list, in_route);
	     next = rlist_next_entry(next, in_route)) {
		next->a_earliest = MAX(next->e, prev->a + prev->s + dist(prev, next));
		assert(next->a == MIN(next->a_earliest, next->l));
		prev = next;
	}
}

int
feasible_ejections_f(va_list ap)
{
	struct route *r = va_arg(ap, struct route *);
	int k_max = va_arg(ap, int);
	int64_t *ps = va_arg(ap, int64_t *);
	struct customer **e = va_arg(ap, struct customer **);
	int *e_size_out = va_arg(ap, int *);
	int64_t *p_best = va_arg(ap, int64_t *);

	if (k_max <= 0)
		return 0;

	struct customer *ne[MAX_N_CUSTOMERS + 2];
	struct customer *s[MAX_N_CUSTOMERS + 2];
	int ne_size = 1, s_size = 0;
	struct customer *ne_last = depot_head(r);
	ne[0] = ne_last;
	ne_last->a_temp = ne_last->a_earliest_temp = ne_last->e;
	*e_size_out = 0;

	init_a_earliest(r);

	struct customer *tmp;
	route_foreach_from(tmp, rlist_next_entry(ne_last, in_route))
		s[s_size++] = tmp;
	for (int i = 0; i < s_size / 2; i++)
		SWAP(s[i], s[s_size - 1 - i]);

	double total_demand;
	total_demand = depot_tail(r)->demand_pf;
	bool capacity_violated = total_demand > p.vc;

	int64_t p_sum = 0;
	int k = 0;

	/** Will be initialized after incr_k */
	struct customer *e_last;
	struct customer *s_first = s[s_size - 1];

	int ejected_infeasibles_count = 0;

	bool incremented_last = false;
	goto incr_k;
	for(;;) {
		if (/** Is better than current optimum */
		    p_sum < *p_best &&
		    /** Doesn't violate time-window constraint */
		    s_first->a_earliest_temp <= s_first->l && s_first->a_temp <= s_first->z && s_first->tw_sf == 0. &&
		    /** Doesn't violate capacity constraint */
		    total_demand <= p.vc) {
			*p_best = p_sum;

			fiber_yield();

			if (fiber_is_cancelled())
				return 0;
		}

		if (s_first->id != 0) {
			/* a) */
			if (p_sum < *p_best && k < k_max) {
				incremented_last = false;
				goto incr_k;
			}
			goto incr_last;
		}

		do {
		/** backtrack */
			s_first = e_last;
			s[s_size++] = s_first;
			--(*e_size_out);

			if (unlikely(k == 1)) {
				*e_size_out = 0;
				return 0;
			}

			e_last = e[*e_size_out - 1];
			p_sum -= ps[s_first->id];
			total_demand += s_first->demand;
			ejected_infeasibles_count -= (s_first->l < s_first->a_earliest);

			while (ne_size > 0) {
				ne_last = ne[ne_size - 1];
				if (ne_last->idx <= e_last->idx)
					break;
				/**
				 * Let's ignore that s_first becomes irrelevant.
				 * We will update it once later.
				 */
				s[s_size++] = ne_last;
				--ne_size;
			}
			ne_last = ne[ne_size - 1];
			s_first = s[s_size - 1];
			--k;
		incr_last:
			incremented_last = true;

			struct customer *prev = ne_last;
			ne_last = e_last;
			ne[ne_size++] = ne_last;
			--(*e_size_out);

			DEBUG_ASSERT_NEAR(ne_last->a_earliest_temp, MAX(ne_last->e,
				prev->a_temp + prev->s + dist(prev, ne_last)));
			DEBUG_ASSERT_NEAR(ne_last->a_temp, MIN(ne_last->a_earliest_temp, ne_last->l));

			p_sum -= ps[ne_last->id];
			total_demand += ne_last->demand;
			ejected_infeasibles_count -= (ne_last->l < ne_last->a_earliest);
			--k;
		incr_k:
			assert(s_first == s[s_size - 1]);
			e_last = s_first;
			--s_size;
			s_first = s[s_size - 1];
			e[(*e_size_out)++] = e_last;
			p_sum += ps[e_last->id];
			total_demand -= e_last->demand;
			ejected_infeasibles_count += (e_last->l < e_last->a_earliest);
			++k;
		/** update */
			assert(ne_last == ne[ne_size - 1]);
			e_last->a_earliest_temp = MAX(e_last->e,
				ne_last->a_temp + ne_last->s + dist(ne_last, e_last));
			e_last->a_temp = MIN(e_last->a_earliest_temp, e_last->l);
			s_first->a_earliest_temp = MAX(s_first->e,
				ne_last->a_temp + ne_last->s + dist(ne_last, s_first));
			s_first->a_temp = MIN(s_first->a_earliest_temp, s_first->l);
		} while (/* b) */
				 ne_last->l < ne_last->a_earliest_temp ||
				 /* c) */
				 (k > 1 && incremented_last &&
				  ejected_infeasibles_count - (e_last->l < e_last->a_earliest) == 0 &&
				  ne_last->a_earliest_temp == ne_last->a_earliest && !capacity_violated));
	}
	unreachable();
}
