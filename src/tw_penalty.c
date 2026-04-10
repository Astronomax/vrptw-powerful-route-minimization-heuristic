#include "tw_penalty.h"

#include "assert.h"

#include "dist.h"
#include "modification.h"
#include "customer.h"
#include "penalty_common.h"
#include "problem.h"
#include "route.h"
#include "utils.h"
#include "penalty_inline.h"

void
tw_penalty_init(struct route *r)
{
	struct customer *next;
	double a_quote;
	struct customer *prev = depot_head(r);
	prev->a = prev->e;
	prev->tw_pf = 0.;
	prev->idx = 0;
	for (next = rlist_next_entry(prev, in_route);
	     !rlist_entry_is_head(next, &r->list, in_route);
	     next = rlist_next_entry(next, in_route)) {
		a_quote = prev->a + prev->s + dist(prev, next);
		next->a = MIN(MAX(a_quote, next->e), next->l);
		next->tw_pf =
			prev->tw_pf + MAX(0., a_quote - next->l);
		/*
		 * TODO: It's strange to do it here. Move this into a
		 * separeate `route_enumerate` function.
		 */
		next->idx = prev->idx + 1;
		prev = next;
	}
	double z_quote;
	next = depot_tail(r);
	next->z = p.depot->l;
	next->tw_sf = 0.;
	for (prev = rlist_prev_entry(next, in_route);
	     !rlist_entry_is_head(prev, &r->list, in_route);
	     prev = rlist_prev_entry(prev, in_route)) {
		z_quote = next->z - prev->s - dist(prev, next);
		prev->z = MIN(MAX(z_quote, prev->e), prev->l);
		prev->tw_sf =
			next->tw_sf + MAX(0., prev->e - z_quote);
		next = prev;
	}
}

void
tw_penalty_update_forward(struct customer *start)
{
	if (start == NULL)
		return;
	struct route *r = start->route;
	assert(r != NULL);

	if (start == depot_head(r)) {
		start->a = start->e;
		start->tw_pf = 0.;
		start->idx = 0;
		start = route_next(start);
		if (rlist_entry_is_head(start, &r->list, in_route))
			return;
	}

	struct customer *prev = route_prev(start);
	for (struct customer *cur = start;
	     !rlist_entry_is_head(cur, &r->list, in_route);
	     cur = route_next(cur)) {
		double a_quote = prev->a + prev->s + dist(prev, cur);
		cur->a = MIN(MAX(a_quote, cur->e), cur->l);
		cur->tw_pf = prev->tw_pf + MAX(0., a_quote - cur->l);
		cur->idx = prev->idx + 1;
		prev = cur;
	}
}

void
tw_penalty_update_backward(struct customer *start)
{
	if (start == NULL)
		return;
	struct route *r = start->route;
	assert(r != NULL);

	if (start == depot_tail(r)) {
		start->z = p.depot->l;
		start->tw_sf = 0.;
		start = route_prev(start);
		if (rlist_entry_is_head(start, &r->list, in_route))
			return;
	}

	struct customer *next = route_next(start);
	for (struct customer *cur = start;
	     !rlist_entry_is_head(cur, &r->list, in_route);
	     cur = route_prev(cur)) {
		double z_quote = next->z - cur->s - dist(cur, next);
		cur->z = MIN(MAX(z_quote, cur->e), cur->l);
		cur->tw_sf = next->tw_sf + MAX(0., cur->e - z_quote);
		next = cur;
	}
}

double
tw_penalty_get_penalty(struct route *r)
{
	return tw_penalty_get_penalty_inline(r);
}

double
tw_penalty_get_insert_penalty(struct customer *v, struct customer *w)
{
	return tw_penalty_get_insert_penalty_inline(v, w);
}

double
tw_penalty_get_insert_delta(struct customer *v, struct customer *w)
{
	return tw_penalty_get_insert_delta_inline(v, w);
}

double
tw_penalty_get_replace_penalty(struct customer *v, struct customer *w)
{
	return tw_penalty_get_replace_penalty_inline(v, w);
}

double
tw_penalty_get_replace_delta(struct customer *v, struct customer *w)
{
	return tw_penalty_get_replace_delta_inline(v, w);
}

double
tw_penalty_get_eject_penalty(struct customer *v)
{
	return tw_penalty_get_eject_penalty_inline(v);
}

double
tw_penalty_get_eject_delta(struct customer *v)
{
	return tw_penalty_get_eject_delta_inline(v);
}

penalty_common_modification_apply_straight_delta(tw)

double
tw_penalty_one_opt_penalty(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_one_opt_penalty_inline(v, w);
}

double
tw_penalty_two_opt_penalty_delta(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_two_opt_penalty_delta_inline(v, w);
}

/** This will probably never be called */
double
tw_penalty_out_relocate_penalty_delta_slow
	(struct customer *v, struct customer *w)
{
	assert(v->route == w->route);
	struct modification m = modification_new(OUT_RELOCATE, v, w);
	return tw_penalty_modification_apply_straight_delta(m);
}

double
tw_penalty_out_relocate_penalty_delta_fast
	(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_out_relocate_penalty_delta_fast_inline(v, w);
}

double
tw_penalty_out_relocate_penalty_delta(struct customer *v, struct customer *w)
{
	/** This branch will probably never be executed */
	if (unlikely(v->route == w->route))
		return tw_penalty_out_relocate_penalty_delta_slow(v, w);
	else
		return tw_penalty_out_relocate_penalty_delta_fast(v, w);
}

/** This will probably never be called */
double
tw_penalty_exchange_penalty_delta_slow(struct customer *v, struct customer *w)
{
	struct modification m = modification_new(EXCHANGE, v, w);
	return tw_penalty_modification_apply_straight_delta(m);
}

double
tw_penalty_exchange_penalty_delta_fast(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_exchange_penalty_delta_fast_inline(v, w);
}

double
tw_penalty_exchange_penalty_delta(struct customer *v, struct customer *w)
{
	/** This branch will probably never be executed */
	if (unlikely(v->route == w->route))
		return tw_penalty_exchange_penalty_delta_slow(v, w);
	else
		return tw_penalty_exchange_penalty_delta_fast(v, w);
}

double
tw_penalty_exchange_penalty_delta_lower_bound(struct customer *v,
					      struct customer *w,
					      bool *exact)
{
	if (unlikely(v == w)) {
		*exact = true;
		return 0.f;
	}
	/** This branch will probably never be executed */
	if (unlikely(v->route != w->route)) {
		*exact = true;
		return tw_penalty_exchange_penalty_delta_fast(v, w);
	}
#define upd_through_seg(c0, c1, c2) do { 			\
	seg[0] = c0; seg[1] = c1; seg[2] = c2;			\
	a = seg[0]->a, a_quote;                     		\
	for (int j = 1; j < 3; j++) {				\
		a_quote = a + seg[j - 1]->s			\
			+ dist(seg[j - 1], seg[j]);		\
		a = MIN(MAX(a_quote, seg[j]->e), seg[j]->l);	\
		p_tw += MAX(0., a_quote - seg[j]->l);		\
	}} while(0)

		struct route *r = v->route;
		if (v->idx > w->idx)
			SWAP(v, w);
		struct customer *v_minus = route_prev(v), *v_plus = route_next(v),
			*w_minus = route_prev(w), *w_plus = route_next(w);
		double p_tw = v_minus->tw_pf;
		struct customer *seg[3];
		double a = 0., a_quote = 0.;
		if (likely(v->idx + 1 < w->idx)) {
			upd_through_seg(v_minus, w, v_plus);
			if (unlikely(fabs(a - v_plus->a) < EPS7)) {
				p_tw += w_minus->tw_pf - v_plus->tw_pf;
				upd_through_seg(w_minus, v, w_plus);
				if (unlikely(fabs(a - w_plus->a) < EPS7)) {
					p_tw += depot_tail(r)->tw_pf - w_plus->tw_pf;
					*exact = true;
					return p_tw - tw_penalty_get_penalty(r);
				}
			}
			*exact = false;
			return p_tw - tw_penalty_get_penalty(r);
		} else {
			upd_through_seg(v_minus, w, v);
			p_tw += w_plus->tw_sf;
			double a_quote_w_plus = a + v->s + dist(v, w_plus);
			p_tw += MAX(0., a_quote_w_plus - w_plus->z);
			*exact = true;
			return p_tw - tw_penalty_get_penalty(r);
		}
#undef upd_through_seg
}
