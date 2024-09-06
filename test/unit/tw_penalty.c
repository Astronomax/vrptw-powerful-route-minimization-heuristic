#include "unit.h"

#include "core/random.h"

#define TW_PENALTY_TEST
#include "tw_penalty.h"
#undef TW_PENALTY_TEST
#include "penalty_test.h"

static const struct penalty_vtab tw_penalty_vtab = {
	/* .penalty_init = */ tw_penalty_init,
	/* .penalty_get_penalty = */ tw_penalty_get_penalty,
	/* .penalty_get_insert_penalty = */ tw_penalty_get_insert_penalty,
	/* .penalty_get_insert_delta = */ tw_penalty_get_insert_delta,
	/* .penalty_get_replace_penalty = */ tw_penalty_get_replace_penalty,
	/* .penalty_get_replace_delta = */ tw_penalty_get_replace_delta,
	/* .penalty_get_eject_penalty = */ tw_penalty_get_eject_penalty,
	/* .penalty_get_eject_delta = */ tw_penalty_get_eject_delta,
	/* .penalty_two_opt_penalty_delta = */ tw_penalty_two_opt_penalty_delta,
	/* .penalty_out_relocate_penalty_delta = */tw_penalty_out_relocate_penalty_delta,
	/* .penalty_exchange_penalty_delta = */tw_penalty_exchange_penalty_delta,
	/* .penalty_exchange_penalty_delta_lower_bound = */tw_penalty_exchange_penalty_delta_lower_bound,
};

int
main(void)
{
	vtab = tw_penalty_vtab;
	random_init();
	random_insertions(100);
	random_ejections(100);
	random_replacements(100);
	random_two_opts(100);
	random_out_relocations(100);
	random_inter_route_exchanges(100);
	random_intra_route_exchanges(100);
	return 0;
}