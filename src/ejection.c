#include "ejection.h"

#include "core/fiber.h"

#include "dist.h"
#include "route.h"

void
ejections_iterator_create(struct ejections_iterator *it,
	struct route *r, const int64_t *ps, int k_max, int64_t p_best)
{
	it->r = r;
	it->ps = ps;
	it->k_max = k_max;
	it->p_best = p_best;
	it->has_next_value = (k_max > 0);
	rlist_create(&it->e);
	rlist_create(&it->ne);
	rlist_create(&it->s);
	it->ne_last = depot_head(r);
	rlist_add_tail_entry(&it->ne, it->ne_last, in_eject);
	it->ne_last->a_temp = it->ne_last->a;
	struct customer *tmp;
	route_foreach_from(tmp, rlist_next_entry(it->ne_last, in_route))
		rlist_add_tail_entry(&it->s, tmp, in_eject);
	it->total_demand = depot_tail(r)->demand_pf;
	it->p_sum = 0;
	it->k = 0;
	/** Will be initialized after incr_k */
	it->e_last = NULL;
	it->s_first = rlist_first_entry(&it->s, struct customer, in_eject);
	/** If not, iteration will crash */
	assert(it->s_first->id != 0);
}

bool
ejections_iterator_next(struct ejections_iterator *it)
{
	if (!it->has_next_value)
		return false;
	if (it->p_best == 0) {
		rlist_create(&it->e);
		it->has_next_value = false;
		return false;
	}

	struct customer *tmp;
	for(;;) {
		if (it->p_sum < it->p_best && it->s_first->id != 0 && it->k < it->k_max)
			goto incr_k;
		if (it->s_first->id != 0)
			goto incr_last;

		do {
		/** backtrack */
			assert(it->e_last == rlist_last_entry(&it->e, struct customer, in_eject));
			it->s_first = it->e_last;
			it->e_last = rlist_prev_entry(it->e_last, in_eject);
			assert(it->e_last != NULL);
			rlist_move_entry(&it->s, it->s_first, in_eject);

			if (unlikely(it->k == 1)) {
				assert(rlist_empty(&it->e));
				it->has_next_value = false;
				return false;
			}

			it->p_sum -= it->ps[it->s_first->id];
			it->total_demand += it->s_first->demand;
			rlist_foreach_entry_safe_reverse(it->ne_last, &it->ne, in_eject, tmp) {
				if (it->ne_last->idx <= it->e_last->idx) break;
				/**
				 * Let's ignore that s_first becomes irrelevant.
				 * We will update it once later.
				 */
				rlist_move_entry(&it->s, it->ne_last, in_eject);
			}
			it->s_first = rlist_first_entry(&it->s, struct customer, in_eject);
			--it->k;
		incr_last:
			assert(it->e_last == rlist_last_entry(&it->e, struct customer, in_eject));
			it->ne_last = it->e_last;
			rlist_move_tail_entry(&it->ne, it->ne_last, in_eject);
			it->p_sum -= it->ps[it->ne_last->id];
			it->total_demand += it->ne_last->demand;
			--it->k;
		incr_k:
			assert(it->s_first == rlist_first_entry(&it->s, struct customer, in_eject));
			it->e_last = it->s_first;
			assert(it->e_last != NULL);
			it->s_first = rlist_next_entry(it->s_first, in_eject);
			rlist_move_tail_entry(&it->e, it->e_last, in_eject);
			it->p_sum += it->ps[it->e_last->id];
			it->total_demand -= it->e_last->demand;
			++it->k;
		/** update */
			assert(it->e_last == rlist_last_entry(&it->e, struct customer, in_eject));
			assert(it->ne_last == rlist_last_entry(&it->ne, struct customer, in_eject));
			assert(it->s_first == rlist_first_entry(&it->s, struct customer, in_eject));
			it->e_last->a_quote = it->ne_last->a_temp + it->ne_last->s +
				dist(it->ne_last, it->e_last);
			it->e_last->a_temp =
				MIN(MAX(it->e_last->a_quote, it->e_last->e), it->e_last->l);
			it->s_first->a_quote = it->ne_last->a_temp + it->ne_last->s +
				dist(it->ne_last, it->s_first);
			it->s_first->a_temp =
				MIN(MAX(it->s_first->a_quote, it->s_first->e), it->s_first->l);
		} while (it->ne_last->l < it->ne_last->a_quote);

		if (/** Is better than current optimum */
			it->p_sum < it->p_best &&
			/** Doesn't violate time-window constraint */
			it->s_first->a_quote <= it->s_first->l &&
			it->s_first->a_temp <= it->s_first->z &&
			it->s_first->tw_sf == 0. &&
			/** Doesn't violate capacity constraint */
			it->total_demand <= p.vc)
		{
			it->p_best = it->p_sum;
			return true;
		}
	}
	unreachable();
	return false;
}
