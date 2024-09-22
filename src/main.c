#include "customer.h"
#include "problem.h"
#include "solution.h"

#include "core/fiber.h"
#include "core/memory.h"

int
main(int argc, char **argv)
{
	global_problem_init();
	solution_init();

	memory_init();
	memory_free();
	return 0;
}
