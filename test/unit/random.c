#include "unit.h"

#include <stdint.h>
#include <string.h>

#include "core/random.h"

int
main(void)
{
	plan(3);

	random_init();

	uint64_t seq1[4];
	uint64_t seq2[4];
	uint64_t seq3[4];
	int64_t bounded1[8];
	int64_t bounded2[8];

	pseudo_random_seed(0x0123456789ABCDEFULL);
	for (int i = 0; i < 4; i++)
		seq1[i] = xoshiro_random();

	pseudo_random_seed(0x0123456789ABCDEFULL);
	for (int i = 0; i < 4; i++)
		seq2[i] = xoshiro_random();

	ok(memcmp(seq1, seq2, sizeof(seq1)) == 0,
	   "xoshiro sequence is repeatable for the same 64-bit seed");

	pseudo_random_seed(0x0123456789ABCDEEULL);
	for (int i = 0; i < 4; i++)
		seq3[i] = xoshiro_random();

	ok(memcmp(seq1, seq3, sizeof(seq1)) != 0,
	   "xoshiro sequence changes with different 64-bit seed");

	pseudo_random_seed(0xDEADBEEFCAFEBABEULL);
	for (int i = 0; i < 8; i++)
		bounded1[i] = pseudo_random_in_range(-7, 23);

	pseudo_random_seed(0xDEADBEEFCAFEBABEULL);
	for (int i = 0; i < 8; i++)
		bounded2[i] = pseudo_random_in_range(-7, 23);

	ok(memcmp(bounded1, bounded2, sizeof(bounded1)) == 0,
	   "bounded pseudo-random sequence is repeatable for the same 64-bit seed");

	random_free();

	return check_plan();
}
