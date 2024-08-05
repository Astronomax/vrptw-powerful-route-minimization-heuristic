#include "tw_penalty.h"

#include "assert.h"

#include "dist.h"
#include "modification.h"
#include "customer.h"
#include "penalty_common.h"
#include "problem.h"
#include "utils.h"

void
tw_penalty_init(struct route *r)
{
	struct customer *next;
	double a_quote;
	struct customer *prev = depot_head(r);
	prev->a = prev->tw_pf = 0.;
	for (next = rlist_next_entry(prev, in_route);
	     !rlist_entry_is_head(next, &r->list, in_route);
	     next = rlist_next_entry(next, in_route)) {
		a_quote = prev->a + prev->s + dist(prev, next);
		next->a = MIN(MAX(a_quote, next->e), next->l);
		next->tw_pf =
			prev->tw_pf + MAX(0., a_quote - next->l);
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

double
tw_penalty_get_penalty(struct route *r)
{
	return depot_tail(r)->tw_pf;
}

double
tw_penalty_get_insert_penalty(struct customer *v, struct customer *w)
{
	struct customer *v_minus = rlist_prev_entry(v, in_route);
	double p_tw = v_minus->tw_pf + v->tw_sf;
	double a_quote_w = v_minus->a + v_minus->s + dist(v_minus, w);
	double z_quote_w = v->z - w->s - dist(w, v);
	p_tw += MAX(0., a_quote_w - w->l);
	p_tw += MAX(0., w->e - z_quote_w);
	double a_w = MIN(MAX(a_quote_w, w->e), w->l);
	double z_w = MIN(MAX(z_quote_w, w->e), w->l);
	p_tw += MAX(0., a_w - z_w);
	return p_tw;
}

double
tw_penalty_get_insert_delta(struct customer *v, struct customer *w)
{
	return tw_penalty_get_insert_penalty(v, w)
		- tw_penalty_get_penalty(v->route);
}

double
tw_penalty_get_replace_penalty(struct customer *v, struct customer *w)
{
	struct customer *v_minus = rlist_prev_entry(v, in_route);
	struct customer *v_plus = rlist_next_entry(v, in_route);
	double p_tw = v_minus->tw_pf + v_plus->tw_sf;
	double a_quote_v = v_minus->a + v_minus->s + dist(v_minus, w);
	double z_quote_v = v_plus->z - w->s - dist(w, v_plus);
	p_tw += MAX(0., a_quote_v - w->l);
	p_tw += MAX(0., w->e - z_quote_v);
	double a_v = MIN(MAX(a_quote_v, w->e), w->l);
	double z_v = MIN(MAX(z_quote_v, w->e), w->l);
	p_tw += MAX(0., a_v - z_v);
	return p_tw;
}

double
tw_penalty_get_replace_delta(struct customer *v, struct customer *w)
{
	return tw_penalty_get_replace_penalty(v, w)
	       - tw_penalty_get_penalty(v->route);
}

double
tw_penalty_get_eject_penalty(struct customer *v)
{
	struct customer *v_minus = rlist_prev_entry(v, in_route);
	struct customer *v_plus = rlist_next_entry(v, in_route);
	double p_tw = v_minus->tw_pf + v_plus->tw_sf;
	double a_quote_v_plus = v_minus->a + v_minus->s
		+ dist(v_minus, v_plus);
	double a_v_plus = MIN(MAX(a_quote_v_plus, v_plus->e), v_plus->l);
	p_tw += MAX(0., a_quote_v_plus - v_plus->l);
	p_tw += MAX(0., a_v_plus - v_plus->z);
	return p_tw;
}

double
tw_penalty_get_eject_delta(struct customer *v)
{
	return tw_penalty_get_eject_penalty(v)
		- tw_penalty_get_penalty(v->route);
}

penalty_common_modification_apply_straight_delta(tw)

double
tw_penalty_one_opt_penalty(struct customer *v, struct customer *w)
{

	assert(v->route != w->route);
	struct customer *w_plus = rlist_next_entry(w, in_route);
	double p_tw = v->tw_pf + w_plus->tw_sf;
	double a_quote_w_plus = v->a + v->s + dist(v, w_plus);
	p_tw += MAX(0., a_quote_w_plus - w_plus->z);
	return p_tw;
}

double
tw_penalty_two_opt_penalty_delta(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return (tw_penalty_one_opt_penalty(v, w)
		- tw_penalty_get_penalty(v->route))
		+ (tw_penalty_one_opt_penalty(w, v)
		- tw_penalty_get_penalty(w->route));
}

/** This will probably never be called */
double
tw_penalty_out_relocate_penalty_delta_slow
	(struct customer *v, struct customer *w)
{
	assert(v->route == w->route);
	struct modification m = {OUT_RELOCATE, v, w};
	return tw_penalty_modification_apply_straight_delta(m);
}

double
tw_penalty_out_relocate_penalty_delta_fast
	(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_get_eject_delta(w)
		+ tw_penalty_get_insert_delta(v, w);
}

double
tw_penalty_out_relocate_penalty_delta(struct customer *v, struct customer *w)
{
	/** This will probably never be called */
	if (unlikely(v->route == w->route))
		return tw_penalty_out_relocate_penalty_delta_slow(v, w);
	else
		return tw_penalty_out_relocate_penalty_delta_fast(v, w);
}

/** This will probably never be called */
double
tw_penalty_exchange_penalty_delta_slow(struct customer *v, struct customer *w)
{
	struct modification m = {EXCHANGE, v, w};
	return tw_penalty_modification_apply_straight_delta(m);
}

double
tw_penalty_exchange_penalty_delta_fast(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return tw_penalty_get_replace_delta(v, w)
		+ tw_penalty_get_replace_delta(w, v);
}

double
tw_penalty_exchange_penalty_delta(struct customer *v, struct customer *w)
{
	/** This will probably never be called */
	if (unlikely(v->route == w->route))
		return tw_penalty_exchange_penalty_delta_slow(v, w);
	else
		return tw_penalty_exchange_penalty_delta_fast(v, w);
}