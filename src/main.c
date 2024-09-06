#include "problem.h"
#include "customer.h"
#include "core/fiber.h"
#include "core/memory.h"

static int
test_fiber_f(va_list ap)
{
	int n = va_arg(ap, int);
	for (int i = 0; i < n; i++) {
		printf("test_fiber_f\n");
		fflush(stdout);
		fiber_yield();
	}
	return 0;
}

int
main(int argc, char **argv)
{
	global_problem_init();

	memory_init();
	fiber_init(fiber_c_invoke);
	struct fiber *f = fiber_new(test_fiber_f);
	fiber_start(f, 2);
	while(!fiber_is_dead(f)) {
		printf("main\n");
		fflush(stdout);
		fiber_call(f);
	}
	memory_free();
	return 0;
}