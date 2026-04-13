#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_INLINE_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_INLINE_H

#include "dist.h"
#include "problem.h"
#include "route.h"
#include "utils.h"

static ALWAYS_INLINE double
tw_penalty_get_penalty_inline(struct route *r)
{
	return depot_tail(r)->tw_pf;
}

static ALWAYS_INLINE double
tw_penalty_get_insert_penalty_inline(struct customer *v, struct customer *w)
{
	struct customer *v_minus = route_prev(v);
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

static ALWAYS_INLINE double
tw_penalty_get_insert_delta_inline(struct customer *v, struct customer *w)
{
	return tw_penalty_get_insert_penalty_inline(v, w) -
	       tw_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
tw_penalty_get_replace_penalty_inline(struct customer *v, struct customer *w)
{
	struct customer *v_minus = route_prev(v);
	struct customer *v_plus = route_next(v);
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

static ALWAYS_INLINE double
tw_penalty_get_replace_delta_inline(struct customer *v, struct customer *w)
{
	return tw_penalty_get_replace_penalty_inline(v, w) -
	       tw_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
tw_penalty_get_eject_penalty_inline(struct customer *v)
{
	struct customer *v_minus = route_prev(v);
	struct customer *v_plus = route_next(v);
	double p_tw = v_minus->tw_pf + v_plus->tw_sf;
	double a_quote_v_plus = v_minus->a + v_minus->s + dist(v_minus, v_plus);
	double a_v_plus = MIN(MAX(a_quote_v_plus, v_plus->e), v_plus->l);
	p_tw += MAX(0., a_quote_v_plus - v_plus->l);
	p_tw += MAX(0., a_v_plus - v_plus->z);
	return p_tw;
}

static ALWAYS_INLINE double
tw_penalty_get_eject_delta_inline(struct customer *v)
{
	return tw_penalty_get_eject_penalty_inline(v) -
	       tw_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
tw_penalty_one_opt_penalty_inline(struct customer *v, struct customer *w)
{
	struct customer *w_plus = route_next(w);
	double p_tw = v->tw_pf + w_plus->tw_sf;
	double a_quote_w_plus = v->a + v->s + dist(v, w_plus);
	p_tw += MAX(0., a_quote_w_plus - w_plus->z);
	return p_tw;
}

static ALWAYS_INLINE double
tw_penalty_two_opt_penalty_delta_inline(struct customer *v, struct customer *w)
{
	return (tw_penalty_one_opt_penalty_inline(v, w) -
		tw_penalty_get_penalty_inline(v->route)) +
	       (tw_penalty_one_opt_penalty_inline(w, v) -
		tw_penalty_get_penalty_inline(w->route));
}

static ALWAYS_INLINE double
tw_penalty_out_relocate_penalty_delta_fast_inline(struct customer *v,
						  struct customer *w)
{
	return tw_penalty_get_eject_delta_inline(w) +
	       tw_penalty_get_insert_delta_inline(v, w);
}

static ALWAYS_INLINE double
tw_penalty_exchange_penalty_delta_fast_inline(struct customer *v,
					      struct customer *w)
{
	return tw_penalty_get_replace_delta_inline(v, w) +
	       tw_penalty_get_replace_delta_inline(w, v);
}

static ALWAYS_INLINE double
c_penalty_get_penalty_inline(struct route *r)
{
	return MAX(0., depot_tail(r)->demand_pf - p.vc);
}

static ALWAYS_INLINE double
c_penalty_get_insert_penalty_inline(struct customer *v, struct customer *w)
{
	return MAX(0., depot_tail(v->route)->demand_pf + w->demand - p.vc);
}

static ALWAYS_INLINE double
c_penalty_get_insert_delta_inline(struct customer *v, struct customer *w)
{
	return c_penalty_get_insert_penalty_inline(v, w) -
	       c_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
c_penalty_get_replace_penalty_inline(struct customer *v, struct customer *w)
{
	return MAX(0., depot_tail(v->route)->demand_pf - v->demand + w->demand -
			   p.vc);
}

static ALWAYS_INLINE double
c_penalty_get_replace_delta_inline(struct customer *v, struct customer *w)
{
	return c_penalty_get_replace_penalty_inline(v, w) -
	       c_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
c_penalty_get_eject_penalty_inline(struct customer *v)
{
	return MAX(0., depot_tail(v->route)->demand_pf - v->demand - p.vc);
}

static ALWAYS_INLINE double
c_penalty_get_eject_delta_inline(struct customer *v)
{
	return c_penalty_get_eject_penalty_inline(v) -
	       c_penalty_get_penalty_inline(v->route);
}

static ALWAYS_INLINE double
c_penalty_one_opt_penalty_inline(struct customer *v, struct customer *w)
{
	struct customer *w_plus = route_next(w);
	return MAX(0., v->demand_pf + w_plus->demand_sf - p.vc);
}

static ALWAYS_INLINE double
c_penalty_two_opt_penalty_delta_inline(struct customer *v, struct customer *w)
{
	return (c_penalty_one_opt_penalty_inline(v, w) -
		c_penalty_get_penalty_inline(v->route)) +
	       (c_penalty_one_opt_penalty_inline(w, v) -
		c_penalty_get_penalty_inline(w->route));
}

static ALWAYS_INLINE double
c_penalty_out_relocate_penalty_delta_inline(struct customer *v,
					    struct customer *w)
{
	return c_penalty_get_eject_delta_inline(w) +
	       c_penalty_get_insert_delta_inline(v, w);
}

static ALWAYS_INLINE double
c_penalty_exchange_penalty_delta_inline(struct customer *v, struct customer *w)
{
	return c_penalty_get_replace_delta_inline(v, w) +
	       c_penalty_get_replace_delta_inline(w, v);
}

#endif // EAMA_ROUTES_MINIMIZATION_HEURISTIC_PENALTY_INLINE_H
