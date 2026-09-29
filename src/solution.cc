#include "solution.h"

#include "dist.h"
#include "modification.h"
#include "penalty_inline.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "core/random.h"
#include "core/exception.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

struct solution_meta {
    customer *idx[0];
};

bool solution_global_initialized = false;

/**
 * Arrays of neighbours sorted by dist from particular customer.
 * There are no depot in this arrays. Depots are processed separately.
 */
int neighbours_sorted[MAX_N_CUSTOMERS][MAX_N_CUSTOMERS];

void
init_neighbours_sorted()
{
	std::vector<customer *> cs(p.n_customers + 1);
	cs[0] = p.depot;
	{
		customer *c;
		rlist_foreach_entry(c, &p.customers, in_route) cs[c->id] = c;
	}
	assert(cs.begin() != cs.end());

#define init_row() {				\
std::sort(cs.begin() + 1, cs.end(),		\
        [c](customer *a, customer *b) {		\
    	return dist(c, a) < dist(c, b); });	\
std::transform(cs.begin() + 1, cs.end(),	\
       std::begin(neighbours_sorted[c->id]),	\
       [](customer *c) { return c->id; });	\
} while(0)
	struct customer *c = p.depot;
	init_row();
	rlist_foreach_entry(c, &p.customers, in_route)
		init_row();
#undef init_row
}

/**
 * pre-calculated data in order to quickly process intra-route
 * out-relocate modifications
 */
struct intra_route_out_relocate_data {
	route *r;
	customer *w;
	customer *id_to_customer[MAX_N_CUSTOMERS];
	double tw_penalty_delta;
};

/*
 * The helpers below replace the former fiber generator. Each `*_try`
 * helper evaluates one or more candidate modifications for a given (v, w)
 * pair, updates the running best (best_m / best_delta) in place, and
 * returns true as soon as the best delta drops to <= early_exit_delta
 * (the early-exit threshold), so the caller can stop scanning.
 *
 * The invariant on entry to every helper is: best_delta > early_exit_delta
 * (otherwise the caller would already have stopped). Hence a helper returns
 * true iff it just improved the best to within the threshold.
 */

static inline bool
update_best(struct modification cand, double delta, double early_exit_delta,
	    struct modification *best_m, double *best_delta)
{
	if (delta < *best_delta) {
		*best_m = cand;
		*best_delta = delta;
	}
	return *best_delta <= early_exit_delta;
}

/**
 * Build a candidate, compute its delta via modification_delta(), and
 * consider it for the best. Used for inter-route candidates (and the
 * intra-route EXCHANGE slow path is NOT taken here -- see below).
 */
static inline bool
try_candidate(struct modification cand, double alpha, double beta,
	      double early_exit_delta,
	      struct modification *best_m, double *best_delta)
{
	if (!modification_applicable(cand))
		return false;
	double delta = modification_delta(cand, alpha, beta);
	return update_best(cand, delta, early_exit_delta, best_m, best_delta);
}

/**
 * Replaces inter_route_modifications(). Same dispatch and candidate order
 * (TWO_OPT, then OUT_RELOCATE, then EXCHANGE), with the same depot
 * special-cases. Early-exit is checked after each candidate, matching the
 * original per-yield fiber_cancel behaviour.
 */
static bool
inter_route_try(customer *v, customer *w, double alpha, double beta,
		double early_exit_delta,
		struct modification *best_m, double *best_delta)
{
	assert(v->route != w->route);
	/*
	 * Most inter-route candidates are generated with depot sentinels.
	 * Avoid constructing modifications that modification_applicable()
	 * would reject immediately.
	 */
	if (w->id == 0) {
		if (w == depot_tail(w->route))
			return false;
		if (v->id != 0)
			return try_candidate(modification_new(TWO_OPT, v, w),
					     alpha, beta, early_exit_delta,
					     best_m, best_delta);
		return false;
	}

	if (v->id == 0)
		return try_candidate(modification_new(OUT_RELOCATE, v, w),
				     alpha, beta, early_exit_delta,
				     best_m, best_delta);

	if (try_candidate(modification_new(TWO_OPT, v, w),
			  alpha, beta, early_exit_delta, best_m, best_delta))
		return true;
	if (try_candidate(modification_new(OUT_RELOCATE, v, w),
			  alpha, beta, early_exit_delta, best_m, best_delta))
		return true;
	return try_candidate(modification_new(EXCHANGE, v, w),
			     alpha, beta, early_exit_delta, best_m, best_delta);
}

/**
 * Replaces the OUT_RELOCATE branch of intra_route_modifications() plus
 * intra_route_out_relocate_init_delta(). The delta is computed on the
 * duped route where w is already ejected:
 *   tw = eject_delta(w) + insert_delta(v_dup, w_dup),  c = 0
 * (intra-route relocate preserves route demand). This must NOT go through
 * modification_delta() on the real route, which would hit the slow mutating
 * path.
 */
static bool
intra_out_relocate_try(intra_route_out_relocate_data *d,
		       customer *v, customer *w,
		       double alpha, double beta, double early_exit_delta,
		       struct modification *best_m, double *best_delta)
{
	assert(w->id == d->w->id);
	struct modification cand = modification_new(OUT_RELOCATE, v, w);
	if (!modification_applicable(cand))
		return false;
	customer *v_dup = d->id_to_customer[v->id];
	assert(modification_applicable(modification_new(INSERT, v_dup, d->w)));
	double tw = d->tw_penalty_delta +
		    tw_penalty_get_insert_delta(v_dup, d->w);
	double delta = alpha * 0. + beta * tw;
	return update_best(cand, delta, early_exit_delta, best_m, best_delta);
}

/**
 * Replaces the EXCHANGE branch of intra_route_modifications(). IMPORTANT:
 * the original generator yields an intra-route EXCHANGE candidate ONLY when
 * tw_penalty_exchange_penalty_delta_lower_bound() reports exact == true
 * (c_penalty_delta is 0 because intra-route exchange preserves route
 * demand). When the lower bound is not exact, the candidate is skipped
 * entirely -- it is NOT computed via the slow path. This preserves that
 * behaviour exactly.
 */
static bool
intra_exchange_try(customer *v, customer *w,
		   double alpha, double beta, double early_exit_delta,
		   struct modification *best_m, double *best_delta)
{
	struct modification cand = modification_new(EXCHANGE, v, w);
	if (!modification_applicable(cand))
		return false;
	bool exact;
	double lb = tw_penalty_exchange_penalty_delta_lower_bound(v, w, &exact);
	if (!exact)
		return false;
	double delta = alpha * 0. + beta * lb;
	return update_best(cand, delta, early_exit_delta, best_m, best_delta);
}

void
intra_route_out_relocate_data_create(
	intra_route_out_relocate_data *data,
	customer *w)
{
	assert(w->id != 0);
	/** WARNING: ALLOCATION! */
	data->r = route_dup(w->route);
	customer *v;
	route_foreach(v, data->r)
		data->id_to_customer[v->id] = v;
	data->w = data->id_to_customer[w->id];
	auto m = modification_new(EJECT, data->w, nullptr);
	assert(modification_applicable(m));
	data->tw_penalty_delta = tw_penalty_get_eject_delta(data->w);
	modification_apply(m);
}

void
intra_route_out_relocate_data_destroy(intra_route_out_relocate_data *data)
{
	/*
	 * `data->w` points into `data->r->customers[]` (it is the duped copy
	 * of w, set in _create). `route_delete(data->r)` frees every customer
	 * in the duped route, excluding `data->w`, because it was ejected.
	 */
	if (data->r != nullptr) {
		route_delete(data->r);
		data->r = nullptr;
	}
	if (data->w != nullptr) {
		customer_delete(data->w);
		data->w = nullptr;
	}
}

void
solution_global_init()
{
	if (unlikely(!solution_global_initialized)) {
		init_neighbours_sorted();
		solution_global_initialized = true;
	}
}

/*
 * SoA layout for the non-depot customers of the infeasible route r, sorted
 * by ascending id. Sorting by id makes dist(v_side, w) a contiguous *row* of
 * the distance matrix (gatherable) and the w-field arrays contiguous loads
 * -- this is what lets the AVX2 kernel below fill its lanes.
 */
struct w_soa {
	int n;		/* real (non-depot) customer count */
	int n_padded;	/* n rounded up to a multiple of 4 (padding lanes) */
	customer *w_ptr[MAX_N_CUSTOMERS];
	int id[MAX_N_CUSTOMERS];
	int wm_id[MAX_N_CUSTOMERS];	/* w_minus->id */
	int wp_id[MAX_N_CUSTOMERS];	/* w_plus->id */
	double e[MAX_N_CUSTOMERS], l[MAX_N_CUSTOMERS], s[MAX_N_CUSTOMERS];
	double demand[MAX_N_CUSTOMERS], demand_pf[MAX_N_CUSTOMERS];
	double tw_pf[MAX_N_CUSTOMERS], a[MAX_N_CUSTOMERS];
	double wm_tw_pf[MAX_N_CUSTOMERS], wm_a[MAX_N_CUSTOMERS], wm_s[MAX_N_CUSTOMERS];
	double wp_tw_sf[MAX_N_CUSTOMERS], wp_z[MAX_N_CUSTOMERS], wp_demand_sf[MAX_N_CUSTOMERS];
	double eject_tw[MAX_N_CUSTOMERS], eject_c[MAX_N_CUSTOMERS];
};

static void
build_w_soa(struct route *r, struct w_soa *soa)
{
	static customer *tmp[MAX_N_CUSTOMERS];
	int n = 0;
	customer *w;
	route_foreach(w, r) {
		if (w->id == 0)
			continue;	/* skip depot_head / depot_tail */
		tmp[n++] = w;
	}
	std::sort(tmp, tmp + n, [](customer *x, customer *y) {
		return x->id < y->id;
	});
	soa->n = n;
	for (int k = 0; k < n; k++) {
		w = tmp[k];
		customer *wm = route_prev(w);
		customer *wp = route_next(w);
		soa->w_ptr[k] = w;
		soa->id[k] = w->id;
		soa->wm_id[k] = wm->id;
		soa->wp_id[k] = wp->id;
		soa->e[k] = w->e; soa->l[k] = w->l; soa->s[k] = w->s;
		soa->demand[k] = w->demand;
		soa->demand_pf[k] = w->demand_pf;
		soa->tw_pf[k] = w->tw_pf; soa->a[k] = w->a;
		soa->wm_tw_pf[k] = wm->tw_pf;
		soa->wm_a[k] = wm->a; soa->wm_s[k] = wm->s;
		soa->wp_tw_sf[k] = wp->tw_sf;
		soa->wp_z[k] = wp->z;
		soa->wp_demand_sf[k] = wp->demand_sf;
		soa->eject_tw[k] = tw_penalty_get_eject_delta_inline(w);
		soa->eject_c[k] = c_penalty_get_eject_delta_inline(w);
	}
	/*
	 * Pad to a multiple of 4 so the AVX2 kernel has no scalar tail. Padding
	 * lanes are masked to +infinity in the kernel (via eject_tw/eject_c and
	 * a blend mask), so they never win the min-reduce and their w_ptr
	 * (NULL) is never dereferenced.
	 */
	int n_padded = (n + 3) & ~3;
	soa->n_padded = n_padded;
	for (int k = n; k < n_padded; k++) {
		soa->w_ptr[k] = nullptr;
		soa->id[k] = 0;
		soa->wm_id[k] = 0;
		soa->wp_id[k] = 0;
		soa->e[k] = 0.; soa->l[k] = 0.; soa->s[k] = 0.;
		soa->demand[k] = 0.;
		soa->demand_pf[k] = 0.;
		soa->tw_pf[k] = 0.; soa->a[k] = 0.;
		soa->wm_tw_pf[k] = 0.;
		soa->wm_a[k] = 0.; soa->wm_s[k] = 0.;
		soa->wp_tw_sf[k] = 0.;
		soa->wp_z[k] = 0.;
		soa->wp_demand_sf[k] = 0.;
		soa->eject_tw[k] = INFINITY;
		soa->eject_c[k] = INFINITY;
	}
}

/*
 * Scalar inter-route batch: fallback for !__AVX2__ and the SIMD tail.
 * For a fixed inter-route position v, evaluate OUT_RELOCATE (always) and
 * EXCHANGE + TWO_OPT (when v is non-depot) for every w in soa, via
 * modification_delta() -- same _fast_inline formulas the AVX2 kernel uses.
 */
static bool
scalar_inter_batch(customer *v, const struct w_soa *soa,
		   double alpha, double beta, double early_exit_delta,
		   struct modification *best_m, double *best_delta)
{
	const bool do_all3 = (v->id != 0);
	for (int k = 0; k < soa->n; k++) {
		customer *w = soa->w_ptr[k];
		if (do_all3) {
			if (try_candidate(modification_new(TWO_OPT, v, w),
					  alpha, beta, early_exit_delta,
					  best_m, best_delta))
				return true;
			if (try_candidate(modification_new(EXCHANGE, v, w),
					  alpha, beta, early_exit_delta,
					  best_m, best_delta))
				return true;
		}
		if (try_candidate(modification_new(OUT_RELOCATE, v, w),
				  alpha, beta, early_exit_delta,
				  best_m, best_delta))
			return true;
	}
	return false;
}

/*
 * AVX2 inter-route batch. For a fixed position v (v->route != r), compute
 * OUT_RELOCATE / EXCHANGE / TWO_OPT deltas for 4 candidates w at a time.
 * depot_tail v (v->id == 0) -> OUT_RELOCATE only. Returns true on early-exit.
 * Formulas mirror penalty_inline.h (verified against modification_delta's
 * _fast_inline paths). total = alpha*c_delta + beta*tw_delta per type.
 */
static bool
simd_inter_batch(customer *v, const struct w_soa *soa, struct route *r,
		 double alpha, double beta, double early_exit_delta,
		 struct modification *best_m, double *best_delta)
{
#if defined(__AVX2__)
	customer *v_minus = route_prev(v);
	const bool do_all3 = (v->id != 0);
	/* route_next(v) is OOB for depot_tail (idx size-1) -- only load it when
	 * v is a real customer (do_all3), since v_plus is unused otherwise. */
	customer *v_plus = do_all3 ? route_next(v) : nullptr;

	const double vm_tw_pf = v_minus->tw_pf;
	const double v_tw_sf  = v->tw_sf;
	const double vm_a = v_minus->a;
	const double vm_s = v_minus->s;
	const double v_z = v->z;
	const int vm_id = v_minus->id;
	const int v_id = v->id;
	const double tw_pen_v = tw_penalty_get_penalty_inline(v->route);
	const double c_pen_v  = c_penalty_get_penalty_inline(v->route);
	const double dtail_v  = depot_tail(v->route)->demand_pf;
	const double pvc = p.vc;
	const double (*dm)[MAX_N_CUSTOMERS + 1] = p.distance_matrix;

	const double v_tw_pf   = do_all3 ? v->tw_pf          : 0.;
	const double v_a       = do_all3 ? v->a              : 0.;
	const double v_s       = do_all3 ? v->s              : 0.;
	const double v_e       = do_all3 ? v->e              : 0.;
	const double v_l       = do_all3 ? v->l              : 0.;
	const double v_demand  = do_all3 ? v->demand         : 0.;
	const double v_demndpf = do_all3 ? v->demand_pf      : 0.;
	const double vp_tw_sf  = do_all3 ? v_plus->tw_sf     : 0.;
	const double vp_z      = do_all3 ? v_plus->z         : 0.;
	const double vp_demsf  = do_all3 ? v_plus->demand_sf : 0.;
	const int vp_id = do_all3 ? v_plus->id : 0;
	const double tw_pen_r = tw_penalty_get_penalty_inline(r);
	const double c_pen_r  = c_penalty_get_penalty_inline(r);
	const double dtail_r  = depot_tail(r)->demand_pf;

	const __m256d z = _mm256_setzero_pd();
#define B(name, val) const __m256d name = _mm256_set1_pd(val)
	B(b_vm_tw_pf, vm_tw_pf); B(b_v_tw_sf, v_tw_sf);
	B(b_vm_a, vm_a); B(b_vm_s, vm_s); B(b_v_z, v_z);
	B(b_tw_pen_v, tw_pen_v); B(b_c_pen_v, c_pen_v);
	B(b_dtail_v, dtail_v); B(b_pvc, pvc);
	B(b_tw_pen_r, tw_pen_r); B(b_c_pen_r, c_pen_r); B(b_dtail_r, dtail_r);
	B(b_alpha, alpha); B(b_beta, beta);
	B(b_v_tw_pf, v_tw_pf); B(b_v_a, v_a); B(b_v_s, v_s);
	B(b_v_e, v_e); B(b_v_l, v_l);
	B(b_v_demand, v_demand); B(b_v_demndpf, v_demndpf);
	B(b_vp_tw_sf, vp_tw_sf); B(b_vp_z, vp_z); B(b_vp_demsf, vp_demsf);
#undef B

	const int n = soa->n;
	const int n_padded = soa->n_padded;
	const __m256d inf_vec = _mm256_set1_pd(INFINITY);
	int i = 0;
	for (; i < n_padded; i += 4) {
		const __m128i ids_w  = _mm_loadu_si128((const __m128i *)(soa->id + i));
		/* dist is symmetric (dm[a][b] == dm[b][a]); gather each column over
		 * the fixed v-side row instead of 4 strided cold loads via set_pd. */
		const __m256d d_vm_w = _mm256_i32gather_pd(&dm[vm_id][0], ids_w, 8);
		const __m256d d_w_v  = _mm256_i32gather_pd(&dm[v_id][0], ids_w, 8);

		const __m256d e = _mm256_loadu_pd(soa->e + i);
		const __m256d l = _mm256_loadu_pd(soa->l + i);
		const __m256d s = _mm256_loadu_pd(soa->s + i);
		const __m256d demand = _mm256_loadu_pd(soa->demand + i);
		const __m256d eject_tw = _mm256_loadu_pd(soa->eject_tw + i);
		const __m256d eject_c  = _mm256_loadu_pd(soa->eject_c + i);

		/* ---- OUT_RELOCATE: eject(w) + insert(v,w) ---- */
		const __m256d aq = _mm256_add_pd(_mm256_add_pd(b_vm_a, b_vm_s), d_vm_w);
		const __m256d zq = _mm256_sub_pd(_mm256_sub_pd(b_v_z, s), d_w_v);
		const __m256d t1 = _mm256_max_pd(z, _mm256_sub_pd(aq, l));
		const __m256d t2 = _mm256_max_pd(z, _mm256_sub_pd(e, zq));
		const __m256d aw = _mm256_min_pd(_mm256_max_pd(aq, e), l);
		const __m256d zw = _mm256_min_pd(_mm256_max_pd(zq, e), l);
		const __m256d t3 = _mm256_max_pd(z, _mm256_sub_pd(aw, zw));
		const __m256d ins_pen_tw = _mm256_add_pd(
			_mm256_add_pd(_mm256_add_pd(b_vm_tw_pf, b_v_tw_sf), t1),
			_mm256_add_pd(t2, t3));
		const __m256d or_tw = _mm256_add_pd(eject_tw,
			_mm256_sub_pd(ins_pen_tw, b_tw_pen_v));
		const __m256d ins_pc = _mm256_max_pd(z,
			_mm256_sub_pd(_mm256_add_pd(b_dtail_v, demand), b_pvc));
		const __m256d or_c = _mm256_add_pd(eject_c,
			_mm256_sub_pd(ins_pc, b_c_pen_v));
		__m256d or_tot = _mm256_add_pd(
			_mm256_mul_pd(b_alpha, or_c),
			_mm256_mul_pd(b_beta, or_tw));

		__m256d ex_tot = or_tot, to_tot = or_tot;
		if (do_all3) {
			/* dm[w_id][vp_id] == dm[vp_id][w_id] (symmetric) -> row gather. */
			const __m256d d_w_vp = _mm256_i32gather_pd(&dm[vp_id][0], ids_w, 8);
			/* dm[wm_id][v_id] == dm[v_id][wm_id] -> gather v_id row by wm_ids. */
			const __m128i ids_wm = _mm_loadu_si128((const __m128i *)(soa->wm_id + i));
			const __m256d d_wm_v = _mm256_i32gather_pd(&dm[v_id][0], ids_wm, 8);
			const __m128i ids_wp = _mm_loadu_si128((const __m128i *)(soa->wp_id + i));
			const __m256d d_v_wp = _mm256_i32gather_pd(&dm[v_id][0], ids_wp, 8);

			const __m256d wm_tw_pf = _mm256_loadu_pd(soa->wm_tw_pf + i);
			const __m256d wm_a = _mm256_loadu_pd(soa->wm_a + i);
			const __m256d wm_s = _mm256_loadu_pd(soa->wm_s + i);
			const __m256d wp_tw_sf = _mm256_loadu_pd(soa->wp_tw_sf + i);
			const __m256d wp_z = _mm256_loadu_pd(soa->wp_z + i);
			const __m256d wp_demsf = _mm256_loadu_pd(soa->wp_demand_sf + i);
			const __m256d w_tw_pf = _mm256_loadu_pd(soa->tw_pf + i);
			const __m256d w_a = _mm256_loadu_pd(soa->a + i);
			const __m256d w_demndpf = _mm256_loadu_pd(soa->demand_pf + i);

			/* ---- EXCHANGE: replace(v,w) + replace(w,v) ---- */
			/* replace(v,w): vm_tw_pf + vp_tw_sf, a_quote=vm_a+vm_s+d_vm_w, z_quote=vp_z - s - d_w_vp */
			const __m256d aqv = _mm256_add_pd(_mm256_add_pd(b_vm_a, b_vm_s), d_vm_w);
			const __m256d zqv = _mm256_sub_pd(_mm256_sub_pd(b_vp_z, s), d_w_vp);
			const __m256d r1 = _mm256_max_pd(z, _mm256_sub_pd(aqv, l));
			const __m256d r2 = _mm256_max_pd(z, _mm256_sub_pd(e, zqv));
			const __m256d av = _mm256_min_pd(_mm256_max_pd(aqv, e), l);
			const __m256d zv = _mm256_min_pd(_mm256_max_pd(zqv, e), l);
			const __m256d r3 = _mm256_max_pd(z, _mm256_sub_pd(av, zv));
			const __m256d rep_vw = _mm256_add_pd(
				_mm256_add_pd(_mm256_add_pd(b_vm_tw_pf, b_vp_tw_sf), r1),
				_mm256_add_pd(r2, r3));
			/* replace(w,v): wm_tw_pf + wp_tw_sf, a_quote=wm_a+wm_s+d_wm_v, z_quote=wp_z - v_s - d_v_wp */
			const __m256d aqwv = _mm256_add_pd(_mm256_add_pd(wm_a, wm_s), d_wm_v);
			const __m256d zqwv = _mm256_sub_pd(_mm256_sub_pd(wp_z, b_v_s), d_v_wp);
			const __m256d s1 = _mm256_max_pd(z, _mm256_sub_pd(aqwv, b_v_l));
			const __m256d s2 = _mm256_max_pd(z, _mm256_sub_pd(b_v_e, zqwv));
			const __m256d awv = _mm256_min_pd(_mm256_max_pd(aqwv, b_v_e), b_v_l);
			const __m256d zwv = _mm256_min_pd(_mm256_max_pd(zqwv, b_v_e), b_v_l);
			const __m256d s3 = _mm256_max_pd(z, _mm256_sub_pd(awv, zwv));
			const __m256d rep_wv = _mm256_add_pd(
				_mm256_add_pd(_mm256_add_pd(wm_tw_pf, wp_tw_sf), s1),
				_mm256_add_pd(s2, s3));
			const __m256d ex_tw = _mm256_add_pd(
				_mm256_sub_pd(rep_vw, b_tw_pen_v),
				_mm256_sub_pd(rep_wv, b_tw_pen_r));
			/* c: replace_c(v,w)=max(0,dtail_v - v_demand + demand - pvc);
			 *    replace_c(w,v)=max(0,dtail_r - demand + v_demand - pvc) */
			const __m256d rc_vw = _mm256_max_pd(z,
				_mm256_sub_pd(_mm256_add_pd(_mm256_sub_pd(b_dtail_v, b_v_demand), demand), b_pvc));
			const __m256d rc_wv = _mm256_max_pd(z,
				_mm256_sub_pd(_mm256_add_pd(_mm256_sub_pd(b_dtail_r, demand), b_v_demand), b_pvc));
			const __m256d ex_c = _mm256_add_pd(
				_mm256_sub_pd(rc_vw, b_c_pen_v),
				_mm256_sub_pd(rc_wv, b_c_pen_r));
			ex_tot = _mm256_add_pd(_mm256_mul_pd(b_alpha, ex_c),
					       _mm256_mul_pd(b_beta, ex_tw));

			/* ---- TWO_OPT: one_opt(v,w) + one_opt(w,v) ---- */
			/* one_opt(v,w)= v_tw_pf + wp_tw_sf + max(0, (v_a+v_s+d_v_wp) - wp_z) */
			const __m256d aqwp = _mm256_add_pd(_mm256_add_pd(b_v_a, b_v_s), d_v_wp);
			const __m256d o_vw = _mm256_add_pd(
				_mm256_add_pd(b_v_tw_pf, wp_tw_sf),
				_mm256_max_pd(z, _mm256_sub_pd(aqwp, wp_z)));
			/* one_opt(w,v)= w_tw_pf + vp_tw_sf + max(0, (w_a+w_s+d_w_vp) - vp_z) */
			const __m256d aqvp = _mm256_add_pd(_mm256_add_pd(w_a, s), d_w_vp);
			const __m256d o_wv = _mm256_add_pd(
				_mm256_add_pd(w_tw_pf, b_vp_tw_sf),
				_mm256_max_pd(z, _mm256_sub_pd(aqvp, b_vp_z)));
			const __m256d to_tw = _mm256_add_pd(
				_mm256_sub_pd(o_vw, b_tw_pen_v),
				_mm256_sub_pd(o_wv, b_tw_pen_r));
			/* c: one_opt_c(v,w)=max(0, v_demndpf + wp_demsf - pvc);
			 *    one_opt_c(w,v)=max(0, w_demndpf + vp_demsf - pvc) */
			const __m256d oc_vw = _mm256_max_pd(z,
				_mm256_sub_pd(_mm256_add_pd(b_v_demndpf, wp_demsf), b_pvc));
			const __m256d oc_wv = _mm256_max_pd(z,
				_mm256_sub_pd(_mm256_add_pd(w_demndpf, b_vp_demsf), b_pvc));
			const __m256d to_c = _mm256_add_pd(
				_mm256_sub_pd(oc_vw, b_c_pen_v),
				_mm256_sub_pd(oc_wv, b_c_pen_r));
			to_tot = _mm256_add_pd(_mm256_mul_pd(b_alpha, to_c),
					       _mm256_mul_pd(b_beta, to_tw));
		}

		/*
		 * Mask out padding lanes (i+k >= n) by forcing their deltas to
		 * +infinity, so they never win the min-reduce and their w_ptr
		 * (NULL) is never dereferenced. For full interior batches every
		 * lane is valid, so sel is all-zero and the blends are no-ops.
		 */
		const __m256d sel = _mm256_set_pd(i + 3 >= n ? -1.0 : 0.0,
						 i + 2 >= n ? -1.0 : 0.0,
						 i + 1 >= n ? -1.0 : 0.0,
						 i + 0 >= n ? -1.0 : 0.0);
		or_tot = _mm256_blendv_pd(or_tot, inf_vec, sel);
		ex_tot = _mm256_blendv_pd(ex_tot, inf_vec, sel);
		to_tot = _mm256_blendv_pd(to_tot, inf_vec, sel);

		/* scalar min-reduce over the 3 type-vectors x 4 lanes */
		double or_a[4], ex_a[4], to_a[4];
		_mm256_storeu_pd(or_a, or_tot);
		_mm256_storeu_pd(ex_a, ex_tot);
		_mm256_storeu_pd(to_a, to_tot);
#ifndef NDEBUG
		/* Verify each SIMD delta against the scalar modification_delta(). */
		for (int k = 0; k < 4; k++) {
			if (i + k >= n)
				continue;	/* padding lane */
			customer *w = soa->w_ptr[i + k];
			double s_or = modification_delta(
				modification_new(OUT_RELOCATE, v, w), alpha, beta);
			if (fabs(s_or - or_a[k]) > 1e-6 * (1.0 + fabs(s_or))) {
				fprintf(stderr, "OUT_RELOCATE delta mismatch: v=%d w=%d "
					"scalar=%.12g simd=%.12g\n",
					v->id, w->id, s_or, or_a[k]);
				abort();
			}
			if (do_all3) {
				double s_ex = modification_delta(
					modification_new(EXCHANGE, v, w), alpha, beta);
				if (fabs(s_ex - ex_a[k]) > 1e-6 * (1.0 + fabs(s_ex))) {
					fprintf(stderr, "EXCHANGE delta mismatch: v=%d w=%d "
						"scalar=%.12g simd=%.12g\n",
						v->id, w->id, s_ex, ex_a[k]);
					abort();
				}
				double s_to = modification_delta(
					modification_new(TWO_OPT, v, w), alpha, beta);
				if (fabs(s_to - to_a[k]) > 1e-6 * (1.0 + fabs(s_to))) {
					fprintf(stderr, "TWO_OPT delta mismatch: v=%d w=%d "
						"scalar=%.12g simd=%.12g\n",
						v->id, w->id, s_to, to_a[k]);
					abort();
				}
			}
		}
#endif
		for (int k = 0; k < 4; k++) {
			if (or_a[k] < *best_delta) {
				*best_m = modification_new(OUT_RELOCATE, v, soa->w_ptr[i+k]);
				*best_delta = or_a[k];
			}
			if (do_all3) {
				if (ex_a[k] < *best_delta) {
					*best_m = modification_new(EXCHANGE, v, soa->w_ptr[i+k]);
					*best_delta = ex_a[k];
				}
				if (to_a[k] < *best_delta) {
					*best_m = modification_new(TWO_OPT, v, soa->w_ptr[i+k]);
					*best_delta = to_a[k];
				}
			}
		}
		if (*best_delta <= early_exit_delta)
			return true;
	}
	(void)scalar_inter_batch;
	return false;
#else
	/* !__AVX2__: inter_batch() routes to scalar_inter_batch at runtime. */
	return scalar_inter_batch(v, soa, alpha, beta, early_exit_delta,
				  best_m, best_delta);
#endif
}

static bool
inter_batch(customer *v, const struct w_soa *soa, struct route *r,
	    double alpha, double beta, double early_exit_delta,
	    struct modification *best_m, double *best_delta, bool simd)
{
	if (simd)
		return simd_inter_batch(v, soa, r, alpha, beta, early_exit_delta, best_m, best_delta);
	return scalar_inter_batch(v, soa, alpha, beta, early_exit_delta, best_m, best_delta);
}

double
solution_find_best_modification(struct solution *s, struct route *r,
				double alpha, double beta,
				double early_exit_delta,
				struct modification *out, bool simd)
{
	if (!solution_global_initialized)
		solution_global_init();

	/*
	 * Per-w scratch for the intra-route out-relocate fast path. r is
	 * nullptr until the first non-depot w is processed; destroy() is a
	 * no-op in that state.
	 */
	intra_route_out_relocate_data out_relocate_current;
	out_relocate_current.r = nullptr;
	out_relocate_current.w = nullptr;

	solution_check_routes(s);

	/* SoA of r's non-depot customers, sorted by id (for contiguous dist). */
	static struct w_soa soa;
	build_w_soa(r, &soa);

	struct modification best_m = modification_new(INSERT, nullptr, nullptr);
	double best_delta = INFINITY;

	/*
	 * Inter-route pass (reversed neighbourhood): for each position v in
	 * every route != r, evaluate OUT_RELOCATE / EXCHANGE / TWO_OPT against
	 * the whole w batch via the SIMD kernel. v->id == 0 (depot_tail) ->
	 * OUT_RELOCATE only. All 3 types are applicable for every inter
	 * non-depot v and every non-depot w (no per-w mask needed).
	 */
	for (int i = 0; i < s->n_routes; i++) {
		struct route *vr = s->routes[i];
		if (vr == r)
			continue;
		for (int j = 1; j < vr->size; j++) {
			customer *v = vr->customers[j];
			if (inter_batch(v, &soa, r, alpha, beta,
					early_exit_delta,
					&best_m, &best_delta, simd))
				goto done;
		}
	}

	/*
	 * Intra-route pass (v in r): keep the scalar helpers with the per-w
	 * route_dup scratch. w outer, v inner over r's positions.
	 */
	for (int k = 0; k < soa.n; k++) {
		customer *w = soa.w_ptr[k];
		intra_route_out_relocate_data_destroy(&out_relocate_current);
		intra_route_out_relocate_data_create(&out_relocate_current, w);
		for (int j = 1; j < r->size; j++) {
			customer *v = r->customers[j];
			if (v == w)
				continue;
			if (intra_out_relocate_try(&out_relocate_current, v, w,
						   alpha, beta, early_exit_delta,
						   &best_m, &best_delta))
				goto done;
			if (intra_exchange_try(v, w, alpha, beta,
					       early_exit_delta,
					       &best_m, &best_delta))
				goto done;
		}
	}

done:
	intra_route_out_relocate_data_destroy(&out_relocate_current);
	*out = best_m;
	return best_delta;
}

solution_meta *
solution_meta_new(rlist *problem_customers)
{
	auto *meta = (solution_meta*)
		xmalloc(sizeof(customer *) * (p.n_customers + 1));
	customer *c, *tmp;
	rlist_foreach_entry_safe(c, problem_customers, in_route, tmp) {
		rlist_del_entry(c, in_route);
		meta->idx[c->id] = c;
	}
	return meta;
}

void
solution_meta_delete(solution_meta *meta)
{
	free(meta);
}

void
solution_print_debug(solution *s)
{
	printf("s->w: %p\n", s->w);
	printf("s->ejection_pool: ");
	for (int i = 0; i < s->ejection_pool_size; i++)
		printf("%p, ", (void *)s->ejection_pool[i]);
	printf("\n");
	printf("s->n_routes: %d\n", s->n_routes);
	customer *c;
	for (int i = 0; i < s->n_routes; i++) {
		route_foreach(c, s->routes[i])
			printf("%p, ", c);
		printf("\n");
	}
	fflush(stdout);
}

void
solution_print(solution *s)
{
	for (int i = 0; i < s->n_routes; i++) {
		customer *c;
		route_foreach(c, s->routes[i])
			printf("%d ", c->id);
		printf("\n");
	}
	fflush(stdout);
}

double
solution_routing_cost(solution *s)
{
	double total = 0.;
	for (int i = 0; i < s->n_routes; i++) {
		route *r = s->routes[i];
		customer *prev = depot_head(r);
		for (customer *curr = route_next(prev);
		     curr != depot_tail(r);
		     curr = route_next(curr)) {
			total += dist(prev, curr);
			prev = curr;
		}
		total += dist(prev, depot_tail(r));
	}
	return total;
}

void
solution_print_incumbent_json(solution *s, long elapsed_ms)
{
	printf(
		"incumbent_solution_json: "
		"{\"elapsed_ms\":%ld,\"num_routes\":%d,\"native_cost\":%.12f,\"routes\":[",
		elapsed_ms,
		s->n_routes,
		solution_routing_cost(s)
	);
	for (int i = 0; i < s->n_routes; i++) {
		if (i > 0)
			printf(",");
		printf("[");
		bool first_customer = true;
		customer *c;
		route_foreach(c, s->routes[i]) {
			if (c->id == 0)
				continue;
			if (!first_customer)
				printf(",");
			printf("%d", c->id);
			first_customer = false;
		}
		printf("]");
	}
	printf("]}\n");
	fflush(stdout);
}

solution *
solution_default(void)
{
	auto *s = (solution *)xmalloc(sizeof(solution) +
		sizeof(struct route *) * p.n_customers);

	s->w = nullptr;

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);
	s->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	s->ejection_pool_size = 0;
	s->n_routes = p.n_customers;
	int i = 0;
	customer *c;
	rlist_foreach_entry(c, &p.customers, in_route) {
		route *r = route_new();
		route_init(r, &s->meta->idx[c->id], 1);
		s->routes[i] = r;
		++i;
	}
	assert(i == p.n_customers);
	return s;
}

solution *
solution_decode(const char *file)
{
	std::string file_string{file};
	std::ifstream f{file_string};
	if (!f.is_open())
		panic("solution_decode: cannot open file '%s'", file);

	/* Parse routes: one route per line, space-separated 1-indexed customer IDs */
	std::vector<std::vector<int>> parsed_routes;
	std::string line;
	while (std::getline(f, line)) {
		/* skip blank lines and comments */
		size_t first = line.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			continue;
		if (line[first] == '#')
			continue;

		std::istringstream iss(line);
		std::vector<int> route_ids;
		int id;
		while (iss >> id)
			route_ids.push_back(id);
		if (route_ids.empty())
			continue;
		parsed_routes.push_back(std::move(route_ids));
	}

	if (parsed_routes.empty())
		panic("solution_decode: file '%s' contains no routes", file);

	/* Validate: all customers present exactly once, no depot, in range */
	bool used[MAX_N_CUSTOMERS + 1];
	memset(used, 0, sizeof(used));
	int total_customers = 0;

	for (int ri = 0; ri < (int)parsed_routes.size(); ri++) {
		if (parsed_routes[ri].empty())
			panic("solution_decode: route %d is empty", ri + 1);
		for (int cid : parsed_routes[ri]) {
			if (cid == 0)
				panic("solution_decode: depot (0) must not appear in initial solution");
			if (cid < 1 || cid > p.n_customers)
				panic("solution_decode: customer ID %d out of range [1, %d]",
				      cid, p.n_customers);
			if (used[cid])
				panic("solution_decode: customer %d appears more than once", cid);
			used[cid] = true;
			total_customers++;
		}
	}

	if (total_customers != p.n_customers) {
		int missing = 0;
		for (int i = 1; i <= p.n_customers; i++)
			if (!used[i]) missing++;
		panic("solution_decode: %d customer(s) missing from initial solution", missing);
	}

	/* Build solution following the same pattern as solution_default() */
	auto *s = (solution *)xmalloc(sizeof(solution) +
		sizeof(struct route *) * p.n_customers);

	s->w = nullptr;

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);
	s->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	s->ejection_pool_size = 0;
	s->n_routes = (int)parsed_routes.size();

	/* Temp array for route_init */
	struct customer *arr[MAX_N_CUSTOMERS];

	for (int i = 0; i < s->n_routes; i++) {
		const auto &rids = parsed_routes[i];
		int n = (int)rids.size();
		for (int j = 0; j < n; j++)
			arr[j] = s->meta->idx[rids[j]];

		route *r = route_new();
		route_init(r, arr, n);
		s->routes[i] = r;
	}

	/* Post-build validation */
	solution_check_missed_customers(s);
	if (!solution_feasible(s))
		panic("solution_decode: imported solution is infeasible "
		      "(time windows or capacity violated)");

	return s;
}

/* TODO: deprecate */
solution *
solution_dup(solution *s)
{
	auto *dup = (solution *)xmalloc(sizeof(solution) +
				       sizeof(struct route *) * p.n_customers);

	RLIST_HEAD(problem_customers);
	problem_customers_dup(&problem_customers);

	dup->meta = solution_meta_new(&problem_customers);
	assert(rlist_empty(&problem_customers));

	dup->w = ((s->w != nullptr) ? dup->meta->idx[s->w->id] : nullptr);

	dup->ejection_pool_size = s->ejection_pool_size;
	for (int i = 0; i < s->ejection_pool_size; i++) {
		customer *c = s->ejection_pool[i];
		assert(c->id != 0);
		dup->ejection_pool[i] = dup->meta->idx[c->id];
	}
	dup->n_routes = s->n_routes;
	customer *c;
	for (int i = 0; i < dup->n_routes; i++) {
		route *r = route_new();
		r->size = s->routes[i]->size;
		route_foreach(c, s->routes[i]) {
			auto c_dup = (c->id == 0) ? customer_dup(c) :
				dup->meta->idx[c->id];
			r->customers[c->idx] = c_dup;
		}
		route_refresh_metadata_from(r, 0);
		route_init_penalty(r);
		route_check(r);
		dup->routes[i] = r;
	}
	return dup;
}

void
solution_move(solution *dst, solution *src)
{
	solution_check_missed_customers(dst);
	/*
	 * `customer::p` is a solver-global ejection-frequency counter, not
	 * transactional solution state: it accumulates across snapshots and
	 * must NOT be reverted by a rollback. `src` is the older snapshot whose
	 * customers still carry stale `p` (copied from the problem master at
	 * solution_dup time), so copy dst's current `p` onto src's customers
	 * before the swap -- after the swap dst owns src's customers (now with
	 * the live `p`), and src (about to be deleted) takes dst's old ones.
	 * idx[0] (depot) is not populated, hence 1..n_customers.
	 */
	for (int id = 1; id <= p.n_customers; id++)
		src->meta->idx[id]->p = dst->meta->idx[id]->p;
	SWAP(dst->w, src->w);
	SWAP(dst->meta, src->meta);
	/* Swap the ejection pools element-wise (size differs in general). */
	for (int i = 0; i < MAX(dst->ejection_pool_size, src->ejection_pool_size); i++)
		SWAP(dst->ejection_pool[i], src->ejection_pool[i]);
	SWAP(dst->ejection_pool_size, src->ejection_pool_size);
	for (int i = 0; i < MAX(dst->n_routes, src->n_routes); i++)
		SWAP(dst->routes[i], src->routes[i]);
	SWAP(dst->n_routes, src->n_routes);
	solution_check_missed_customers(src);
	solution_delete(src);
}

void
solution_delete(solution *s)
{
	solution_meta_delete(s->meta);
	free(s->w);
	for (int i = 0; i < s->ejection_pool_size; i++)
		customer_delete(s->ejection_pool[i]);
	for (int i = 0; i < s->n_routes; i++)
		route_delete(s->routes[i]);
	free(s);
}

double
solution_penalty(struct solution *s, double alpha, double beta)
{
	double penalty = 0.;
	for(int i = 0; i < s->n_routes; i++)
		penalty += route_penalty(s->routes[i], alpha, beta);
	return penalty;
}

bool
solution_feasible(solution *s)
{
	for(int i = 0; i < s->n_routes; i++)
		if (!route_feasible(s->routes[i]))
			return false;
	return true;
}

/*
int
split_by_feasibility(solution *s)
{
	int infeasible = 0;
	for (int i = 0; i < s->n_routes; i++) {
		if (!route_feasible(s->routes[i])) {
			SWAP(s->routes[infeasible], s->routes[i]);
			++infeasible;
		}
	}
	return infeasible;
}
*/

struct modification
solution_find_feasible_insertion(struct solution *s, struct customer *w)
{
	assert(is_ejected(w));
	if (!solution_global_initialized)
		solution_global_init();
#define check_insertion() do {						\
	struct modification m = modification_new(INSERT, v, w);		\
	if (modification_applicable(m)) {				\
		double penalty = modification_delta(m, 1., 1.);		\
		if (penalty < EPS5) {					\
			++n_feasible_insertions;			\
			if (randint(1, n_feasible_insertions) == 1)	\
				selected = m;				\
		}							\
	}								\
} while (0)
	int n_feasible_insertions = 0;
	struct modification selected = modification_new(INSERT, nullptr, w);
	customer *v;
	for (int i = 0; i < p.n_customers; i++) {
		int id = neighbours_sorted[w->id][i];
		assert(id != 0);
		v = s->meta->idx[id];
		check_insertion();
	}
	for (int i = 0; i < s->n_routes; i++) {
		v = depot_tail(s->routes[i]);
		check_insertion();
	}
#undef check_insertion
	return selected;
}

struct modification
solution_find_optimal_insertion(struct solution *s, struct customer *w,
				double alpha, double beta)
{
	assert(is_ejected(w));
	struct customer *v;
	struct modification opt_modification =
		modification_new(INSERT, nullptr, nullptr);
	double opt_penalty = INFINITY;
	for(int i = 0; i < s->n_routes; i++) {
		for (int j = 1; j < s->routes[i]->size; j++) {
			v = s->routes[i]->customers[j];
			struct modification m = modification_new(INSERT, v, w);
			double penalty = modification_delta(m, alpha, beta);
			if (penalty < opt_penalty) {
				if (penalty < EPS5)
					return m;
				opt_modification = m;
				opt_penalty = penalty;
			}
		}
	}
	return opt_modification;
}

struct customer *
solution_find_customer_by_id(struct solution *s, int id)
{
	return s->meta->idx[id];
}

void
solution_eliminate_random_route(struct solution *s)
{
	int route_idx = randint(0, s->n_routes - 1);
	struct route *r = s->routes[route_idx];
	SWAP(s->routes[route_idx], s->routes[s->n_routes - 1]);
	--s->n_routes;
	for (int i = 1; i + 1 < r->size; i++) {
		struct customer *c = r->customers[i];
		c->route = nullptr;
		c->idx = -1;
		s->ejection_pool[s->ejection_pool_size++] = c;
	}
	customer_delete(depot_head(r));
	customer_delete(depot_tail(r));
	r->size = 0;
	route_delete(r);
}
