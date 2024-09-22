#include "modification.h"
#include "small_extra/rlist_extra.h"

int
modification_applicable(struct modification m)
{
	struct route *v_route, *w_route;
	v_route = m.v->route;
	/** Possible only in the case of EJECT */
	w_route = (m.w == NULL) ? NULL : m.w->route;
	switch (m.type) {
	case TWO_OPT:
		if (v_route == w_route)
			return -1;
		if (depot_tail(m.v->route) == m.v)
			return -1;
		if (depot_tail(m.w->route) == m.w)
			return -1;
		return 0;
	case OUT_RELOCATE:
		if (w_route == NULL)
			return -1;
	case INSERT:
		if (m.w->id == 0)
			return -1;
		if (v_route == NULL)
			return -1;
		/**
		 * A customer can be inserted before a depot, but only
		 * before the one at the end of the path
		 */
		if (depot_head(v_route) == m.v)
			return -1;
		return 0;
	case EXCHANGE:
		if (m.v->id == 0 || m.w->id == 0)
			return -1;
		if (v_route == NULL || w_route == NULL)
			return -1;
		return 0;
	case EJECT:
		if (m.v->id == 0)
			return -1;
		if (v_route == NULL)
			return -1;
		return 0;
	default:
		unreachable();
	}
}

void
modification_apply(struct modification m)
{
	assert(modification_applicable(m) == 0);
	struct route *v_route, *w_route;
	v_route = m.v->route;
	/** Possible only in the case of EJECT */
	w_route = (m.w == NULL) ? NULL : m.w->route;
	switch (m.type) {
	case TWO_OPT: {
		rlist_swap_tails(&m.v->route->list,
				 &m.w->route->list,
				 &m.v->in_route,
				 &m.w->in_route);
		struct customer *c;
		for (c = rlist_next_entry(m.w, in_route);
		     !rlist_entry_is_head(c, &w_route->list, in_route);
		     c = rlist_next_entry(c, in_route))
			c->route = w_route;
		for (c = rlist_next_entry(m.v, in_route);
		     !rlist_entry_is_head(c, &v_route->list, in_route);
		     c = rlist_next_entry(c, in_route))
			c->route = v_route;
		break;
	}
	case OUT_RELOCATE: {
		if (m.v == m.w)
			return;
		rlist_move_entry(
			rlist_prev(&m.v->in_route), m.w, in_route);
		m.w->route = v_route;
		break;
	}
	case EXCHANGE: {
		if (m.v == m.w)
			return;
		rlist_swap_items(&m.v->in_route, &m.w->in_route);
		SWAP(m.v->route, m.w->route);
		break;
	}
	case INSERT: {
		rlist_move_entry(
			rlist_prev(&m.v->in_route), m.w, in_route);
		m.w->route = v_route;
		route_init_penalty(v_route);
		route_check(v_route);
		/**
		 * This will probably never be true.
		 * Most likely INSERT will always be used only after EJECT.
		 */
		if (unlikely(w_route != NULL)) {
			route_init_penalty(w_route);
			route_check(w_route);
		}
		return;
	}
	case EJECT: {
		rlist_del_entry(m.v, in_route);
		m.v->route = NULL;
		route_init_penalty(v_route);
		route_check(v_route);
		return;
	}
	default:
		unreachable();
	}
	route_init_penalty(v_route);
	route_init_penalty(w_route);
	route_check(v_route);
	route_check(w_route);
}

double
modification_delta(struct modification m, double alpha, double beta)
{
	assert(modification_applicable(m) == 0);
	switch (m.type) {
	case TWO_OPT:
		return alpha * tw_penalty_two_opt_penalty_delta(m.v, m.w)
			+ beta * c_penalty_two_opt_penalty_delta(m.v, m.w);
	case OUT_RELOCATE:
		return alpha * tw_penalty_out_relocate_penalty_delta(m.v, m.w)
		       + beta * c_penalty_out_relocate_penalty_delta(m.v, m.w);
	case EXCHANGE:
		return alpha * tw_penalty_exchange_penalty_delta(m.v, m.w)
		       + beta * c_penalty_exchange_penalty_delta(m.v, m.w);
	case INSERT:
		return alpha * tw_penalty_get_insert_delta(m.v, m.w)
		       + beta * c_penalty_get_insert_delta(m.v, m.w);
	case EJECT:
		return alpha * tw_penalty_get_eject_delta(m.v)
		       + beta * c_penalty_get_eject_delta(m.v);
	default:
		unreachable();
	}
}
