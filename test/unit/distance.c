#include "unit.h"

#include "core/random.h"

#include "distance.h"

#include "penalty_test.h"

static const struct penalty_vtab distance_vtab = {
	/* .penalty_init = */ distance_init,
	/* .penalty_get_penalty = */ distance_get_distance,
	/* .penalty_get_insert_penalty = */ distance_get_insert_distance,
	/* .penalty_get_insert_delta = */ distance_get_insert_delta,
	/* .penalty_get_replace_penalty = */ distance_get_replace_distance,
	/* .penalty_get_replace_delta = */ distance_get_replace_delta,
	/* .penalty_get_eject_penalty = */ distance_get_eject_distance,
	/* .penalty_get_eject_delta = */ distance_get_eject_delta,
	/* .penalty_two_opt_penalty_delta = */ distance_two_opt_distance_delta,
	/* .penalty_out_relocate_penalty_delta = */distance_out_relocate_distance_delta,
	/* .penalty_exchange_penalty_delta = */distance_exchange_distance_delta,
	/* .tw_penalty_exchange_penalty_delta_lower_bound = */NULL,
};

int
main(void)
{
	vtab = distance_vtab;
	random_init();
	random_insertions(100);
	random_ejections(100);
	random_replacements(100);
	random_two_opts(100);
	random_out_relocations(100);
	random_inter_route_exchanges(100);
	return 0;
}