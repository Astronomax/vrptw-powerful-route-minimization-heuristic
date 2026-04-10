#include "c_penalty.h"

#include "assert.h"
#include "math.h"

#include "modification.h"
#include "penalty_common.h"
#include "problem.h"
#include "route.h"
#include "utils.h"
#include "penalty_inline.h"

void
c_penalty_init(struct route *r)
{
	struct customer *next;
	struct customer *prev = depot_head(r);
	prev->demand_pf = prev->demand;
	for (next = rlist_next_entry(prev, in_route);
	     !rlist_entry_is_head(next, &r->list, in_route);
	     next = rlist_next_entry(next, in_route)) {
		next->demand_pf =
			prev->demand_pf + next->demand;
		prev = next;
	}
	next = depot_tail(r);
	next->demand_sf = next->demand;
	for (prev = rlist_prev_entry(next, in_route);
	     !rlist_entry_is_head(prev, &r->list, in_route);
	     prev = rlist_prev_entry(prev, in_route)) {
		prev->demand_sf =
			next->demand_sf + prev->demand;
		next = prev;
	}
}

void
c_penalty_update_forward(struct customer *start)
{
	if (start == NULL)
		return;
	struct route *r = start->route;
	assert(r != NULL);

	if (start == depot_head(r)) {
		start->demand_pf = start->demand;
		start = route_next(start);
		if (rlist_entry_is_head(start, &r->list, in_route))
			return;
	}

	struct customer *prev = route_prev(start);
	for (struct customer *cur = start;
	     !rlist_entry_is_head(cur, &r->list, in_route);
	     cur = route_next(cur)) {
		cur->demand_pf = prev->demand_pf + cur->demand;
		prev = cur;
	}
}

void
c_penalty_update_backward(struct customer *start)
{
	if (start == NULL)
		return;
	struct route *r = start->route;
	assert(r != NULL);

	if (start == depot_tail(r)) {
		start->demand_sf = start->demand;
		start = route_prev(start);
		if (rlist_entry_is_head(start, &r->list, in_route))
			return;
	}

	struct customer *next = route_next(start);
	for (struct customer *cur = start;
	     !rlist_entry_is_head(cur, &r->list, in_route);
	     cur = route_prev(cur)) {
		cur->demand_sf = next->demand_sf + cur->demand;
		next = cur;
	}
}

double
c_penalty_get_penalty(struct route *r)
{
	return c_penalty_get_penalty_inline(r);
}

double
c_penalty_get_insert_penalty(struct customer *v, struct customer *w)
{
	assert(v->route != NULL);
	return c_penalty_get_insert_penalty_inline(v, w);
}

double
c_penalty_get_insert_delta(struct customer *v, struct customer *w)
{
	return c_penalty_get_insert_delta_inline(v, w);
}

double
c_penalty_get_replace_penalty(struct customer *v, struct customer *w)
{
	assert(v->route != NULL);
	return c_penalty_get_replace_penalty_inline(v, w);
}

double
c_penalty_get_replace_delta(struct customer *v, struct customer *w)
{
	return c_penalty_get_replace_delta_inline(v, w);
}

double
c_penalty_get_eject_penalty(struct customer *v)
{
	assert(v->route != NULL);
	return c_penalty_get_eject_penalty_inline(v);
}

double
c_penalty_get_eject_delta(struct customer *v)
{
	return c_penalty_get_eject_delta_inline(v);
}

/** This probably doesn't make sense for c_penalty and will never be used */
penalty_common_modification_apply_straight_delta(c)

double
c_penalty_one_opt_penalty(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return c_penalty_one_opt_penalty_inline(v, w);
}

double
c_penalty_two_opt_penalty_delta(struct customer *v, struct customer *w)
{
	assert(v->route != w->route);
	return c_penalty_two_opt_penalty_delta_inline(v, w);
}

double
c_penalty_out_relocate_penalty_delta(struct customer *v, struct customer *w)
{
	/** This branch will probably never be executed */
	if (unlikely(v->route == w->route))
		return 0.f;
	else
		return c_penalty_out_relocate_penalty_delta_inline(v, w);
}

double
c_penalty_exchange_penalty_delta(struct customer *v, struct customer *w)
{
	/** This branch will probably never be executed */
	if (unlikely(v->route == w->route))
		return 0.f;
	else
		return c_penalty_exchange_penalty_delta_inline(v, w);
}
