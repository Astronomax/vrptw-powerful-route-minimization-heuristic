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
	struct customer *prev = depot_head(r);
	prev->demand_pf = prev->demand;
	for (int i = 1; i < r->size; i++) {
		struct customer *next = r->customers[i];
		next->demand_pf =
			prev->demand_pf + next->demand;
		prev = next;
	}
	struct customer *next = depot_tail(r);
	next->demand_sf = next->demand;
	for (int i = r->size - 2; i >= 0; i--) {
		prev = r->customers[i];
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
		if (start->idx >= r->size)
			return;
	}

	struct customer *prev = route_prev(start);
	for (int i = start->idx; i < r->size; i++) {
		struct customer *cur = r->customers[i];
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
		if (start->idx < 0)
			return;
	}

	struct customer *next = route_next(start);
	for (int i = start->idx; i >= 0; i--) {
		struct customer *cur = r->customers[i];
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
