#include "unit.h"

#include "core/random.h"

#define C_PENALTY_TEST
#include "c_penalty.h"
#undef C_PENALTY_TEST
#include "penalty_test.h"

static const struct penalty_vtab c_penalty_vtab = {
	/* .penalty_init = */ c_penalty_init,
	/* .penalty_get_penalty = */ c_penalty_get_penalty,
	/* .penalty_get_insert_penalty = */ c_penalty_get_insert_penalty,
	/* .penalty_get_insert_delta = */ c_penalty_get_insert_delta,
	/* .penalty_get_replace_penalty = */ c_penalty_get_replace_penalty,
	/* .penalty_get_replace_delta = */ c_penalty_get_replace_delta,
	/* .penalty_get_eject_penalty = */ c_penalty_get_eject_penalty,
	/* .penalty_get_eject_delta = */ c_penalty_get_eject_delta,
	/* .penalty_two_opt_penalty_delta = */ c_penalty_two_opt_penalty_delta,
	/* .penalty_out_relocate_penalty_delta = */c_penalty_out_relocate_penalty_delta,
	/* .penalty_exchange_penalty_delta = */c_penalty_exchange_penalty_delta,
};

int
main(void)
{
	vtab = c_penalty_vtab;
	random_init();
	random_insertions(100);
	random_ejections(100);
	random_replacements(100);
	random_two_opts(100);
	random_out_relocations(100);
	random_exchanges(100);
	return 0;
}