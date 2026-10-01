#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H

#include "small/rlist.h"

#include "customer.h"
#include "modification.h"
#include "random_utils.h"
#include "route.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct solution_meta;

/*
 * Squeeze neighbourhood strategy for solution_find_best_modification.
 *   SQUEEZE_NEAR      - consider only the n_near nearest neighbours of each w
 *                       (the original heuristic, lighter per call).
 *   SQUEEZE_FULL_FAST - full O(n*|r|) neighbourhood, AVX2 vectorized kernel.
 *   SQUEEZE_FULL_SLOW - full O(n*|r|) neighbourhood, scalar kernel.
 */
typedef enum {
	SQUEEZE_NEAR,
	SQUEEZE_FULL_FAST,
	SQUEEZE_FULL_SLOW,
} squeeze_mode;

struct solution {
	struct customer *w;
	struct solution_meta *meta;
	/*
	 * Ejection pool: a LIFO stack of ejected customers (those removed from
	 * routes and awaiting re-insertion). Fixed-size array + size counter,
	 * sized for the worst case of every customer being ejected at once.
	 * Replaces the former intrusive rlist, so customer no longer carries an
	 * `in_eject` link.
	 */
	struct customer *ejection_pool[MAX_N_CUSTOMERS + 2];
	int ejection_pool_size;
	int n_routes;
	struct route *routes[0];
};

void
solution_global_init();

/**
 * Iterate all candidate modifications that move a vertex of route `r` to
 * another position (inter- and intra-route OUT_RELOCATE / EXCHANGE /
 * TWO_OPT), and return the one with the minimum penalty delta.
 *
 * Writes the best modification into `*out` and returns its delta
 * (alpha * c_penalty_delta + beta * tw_penalty_delta). Returns INFINITY
 * (with `*out` left as a null INSERT) when no applicable candidate exists.
 *
 * `early_exit_delta` short-circuits the search: as soon as the running best
 * delta is <= early_exit_delta, the function returns immediately. Pass
 * -v_route_penalty + EPS5 from the caller to stop once an improving move is
 * found.
 *
 * Replaces the former fiber generator solution_modification_neighbourhood_f.
 */
double
solution_find_best_modification(struct solution *s, struct route *r,
				int n_near, double alpha, double beta,
				double early_exit_delta,
				struct modification *out, squeeze_mode mode);

struct solution *
solution_default(void);

/**
 * Build a solution from an external file.
 * File format: one route per line, space-separated 1-indexed customer IDs.
 * Panics on validation failure or infeasible solution.
 */
struct solution *
solution_decode(const char *file);

/* TODO: deprecate */
struct solution *
solution_dup(struct solution *s);

void
solution_move(struct solution *dst, struct solution *src);

void
solution_delete(struct solution *s);

double
solution_penalty(struct solution *s, double alpha, double beta);

bool
solution_feasible(struct solution *s);

//int
//split_by_feasibility(struct solution *s);

void
solution_print(struct solution *s);

double
solution_routing_cost(struct solution *s);

void
solution_print_incumbent_json(struct solution *s, long elapsed_ms);

void ALWAYS_INLINE
solution_check_routes(struct solution *s)
{
	(void)s;
#ifndef NDEBUG
	for (int i = 0; i < s->n_routes; i++)
		route_check(s->routes[i]);
#endif
}

void ALWAYS_INLINE
solution_check_missed_customers(struct solution *s) {
	(void)s;
#ifndef NDEBUG
	static bool used[MAX_N_CUSTOMERS + 1];
	for (int i = 0; i <= p.n_customers; i++)
		used[i] = false;
	int cnt = 0;
	for (int i = 0; i < s->n_routes; i++) {
		struct customer *c;
		route_foreach(c, s->routes[i]) {
			if (!used[c->id]) cnt++;
			used[c->id] = true;
		}
	}
	struct customer *c;
	for (int i = 0; i < s->ejection_pool_size; i++) {
		c = s->ejection_pool[i];
		if (!used[c->id]) cnt++;
		used[c->id] = true;
	}
	if (s->w && !used[s->w->id]) cnt++;
	assert(cnt == p.n_customers + 1);
#endif
}

struct modification
solution_find_feasible_insertion(struct solution *s, struct customer *w);

struct modification
solution_find_optimal_insertion(struct solution *s, struct customer *w,
				double alpha, double beta);

struct customer *
solution_find_customer_by_id(struct solution *s, int id);

void
solution_eliminate_random_route(struct solution *s);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_SOLUTION_H
