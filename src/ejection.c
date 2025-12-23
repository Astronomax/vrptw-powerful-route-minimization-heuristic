#include "ejection.h"

#include "core/fiber.h"

#include "dist.h"
#include "route.h"

void assert_near(double lhs, double rhs) {
	assert(fabs(lhs-rhs)<1e-5);
}

int
feasible_ejections_f(va_list ap)
{
	struct route *r = va_arg(ap, struct route *);
	int k_max = va_arg(ap, int);
	int64_t *ps = va_arg(ap, int64_t *);
	struct rlist *e = va_arg(ap, struct rlist *);
	int64_t *p_best = va_arg(ap, int64_t *);

	if (k_max <= 0)
		return 0;

	RLIST_HEAD(ne);
	RLIST_HEAD(s);
	struct customer *ne_last = depot_head(r);
	rlist_add_tail_entry(&ne, ne_last, in_eject_temp);
	ne_last->a_temp = ne_last->a_quote = ne_last->e;

	struct customer *tmp;
	route_foreach_from(tmp, rlist_next_entry(ne_last, in_route))
		rlist_add_tail_entry(&s, tmp, in_eject_temp);

	double total_demand;
	total_demand = depot_tail(r)->demand_pf;
	int64_t p_sum = 0;
	int k = 0;

	/** Will be initialized after incr_k */
	struct customer *e_last;
	struct customer *s_first = rlist_first_entry(&s, struct customer, in_eject_temp);

	goto incr_k;
	for(;;) {
		if (/** Is better than current optimum */
		    p_sum < *p_best &&
		    /** Doesn't violate time-window constraint */
		    s_first->a_quote <= s_first->l && s_first->a_temp <= s_first->z && s_first->tw_sf == 0. &&
		    /** Doesn't violate capacity constraint */
		    total_demand <= p.vc) {
			*p_best = p_sum;

			struct customer *ne_first = rlist_first_entry(&ne, struct customer, in_eject_temp);
			assert(ne_first->id == 0);
			assert_near(ne_first->a_temp, ne_first->a_quote);
			assert_near(ne_first->a_quote, ne_first->e);
			rlist_foreach_entry(tmp, &ne, in_eject_temp) {
				if (tmp == ne_first) continue;
				struct customer *prev = rlist_prev_entry(tmp, in_eject_temp);
				assert_near(tmp->a_quote, prev->a_temp + prev->s + dist(prev, tmp));
				assert(tmp->a_quote <= tmp->l);
				assert_near(tmp->a_temp, MIN(MAX(tmp->a_quote, tmp->e), tmp->l));
			}
			assert_near(s_first->a_quote, ne_last->a_temp + ne_last->s + dist(ne_last, s_first));
			assert(s_first->a_quote <= s_first->l);
			assert_near(s_first->a_temp, MIN(MAX(s_first->a_quote, s_first->e), s_first->l));
			
			rlist_foreach_entry(tmp, &s, in_eject_temp) {
				if (tmp == s_first) continue;
				struct customer *prev = rlist_prev_entry(tmp, in_eject_temp);
				tmp->a_quote = prev->a_temp + prev->s + dist(prev, tmp);
				assert(tmp->a_quote <= tmp->l);
				tmp->a_temp = MIN(MAX(tmp->a_quote, tmp->e), tmp->l);
			}

			fiber_yield();

			if (fiber_is_cancelled()) {
				rlist_create(e);
				return 0;
			}
		}

		if (p_sum < *p_best && s_first->id != 0 && k < k_max)
			goto incr_k;
		if (s_first->id != 0) goto incr_last;

		do {
		/** backtrack */
			assert(e_last == rlist_last_entry(e, struct customer, in_eject_temp));
			s_first = e_last;
			e_last = rlist_prev_entry(e_last, in_eject_temp);
			rlist_move_entry(&s, s_first, in_eject_temp);

			if (unlikely(k == 1)) {
				assert(rlist_empty(e));
				return 0;
			}

			p_sum -= ps[s_first->id];
			total_demand += s_first->demand;
			rlist_foreach_entry_safe_reverse(ne_last, &ne, in_eject_temp, tmp) {
				if (ne_last->idx <= e_last->idx) break;
				/**
				 * Let's ignore that s_first becomes irrelevant.
				 * We will update it once later.
				 */
				rlist_move_entry(&s, ne_last, in_eject_temp);
			}
			s_first = rlist_first_entry(&s, struct customer, in_eject_temp);
			--k;
		incr_last:
			assert(e_last == rlist_last_entry(e, struct customer, in_eject_temp));
			
			struct customer *prev = ne_last;

			ne_last = e_last;
			rlist_move_tail_entry(&ne, ne_last, in_eject_temp);

			assert(prev == rlist_prev_entry(ne_last, in_eject_temp));
			assert_near(ne_last->a_quote, prev->a_temp + prev->s + dist(prev, ne_last));
			assert_near(ne_last->a_temp, MIN(MAX(ne_last->a_quote, ne_last->e), ne_last->l));

			p_sum -= ps[ne_last->id];
			total_demand += ne_last->demand;
			--k;
		incr_k:
			assert(s_first == rlist_first_entry(&s, struct customer, in_eject_temp));
			e_last = s_first;
			s_first = rlist_next_entry(s_first, in_eject_temp);
			rlist_move_tail_entry(e, e_last, in_eject_temp);
			p_sum += ps[e_last->id];
			total_demand -= e_last->demand;
			++k;
		/** update */
			assert(e_last == rlist_last_entry(e, struct customer, in_eject_temp));
			assert(ne_last == rlist_last_entry(&ne, struct customer, in_eject_temp));
			assert(s_first == rlist_first_entry(&s, struct customer, in_eject_temp));
			e_last->a_quote = ne_last->a_temp + ne_last->s + dist(ne_last, e_last);
			e_last->a_temp = MIN(MAX(e_last->a_quote, e_last->e), e_last->l);
			s_first->a_quote = ne_last->a_temp + ne_last->s + dist(ne_last, s_first);
			s_first->a_temp = MIN(MAX(s_first->a_quote, s_first->e), s_first->l);
		} while (ne_last->l < ne_last->a_quote);
	}
	unreachable();
}
