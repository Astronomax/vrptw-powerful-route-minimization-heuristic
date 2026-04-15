#include "cli.h"

#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <errno.h>
#include <limits.h>

#include "core/diag.h"

static int arg_index;
static int arg_count;
static const char **args;
static const char *current_arg;

struct cli_options options;

static const char *log_levels[3] = {
	[LOGLEVEL_NONE] = "none",
	[LOGLEVEL_NORMAL] = "normal",
	[LOGLEVEL_VERBOSE] = "verbose",
};

static void
usage(void)
{
	printf("Usage: %s <file1> <file2> [<options>]\nfile1 - problem statement, file2 - where to output the solution\n", args[0]);
	printf("\n");
	printf("Options:\n");
	printf("  --beta_correction       - Enables beta-correction mechanism.\n");
	printf("  --log_level <option>    - Log level: none, normal, verbose.\n");
	printf("  --n_near <value>        - Sets the preferred n_near.\n");
	printf("  --k_max <value>         - Sets the preferred k_max.\n");
	printf("  --t_max <value>         - Sets the preferred t_max (in secs).\n");
	printf("  --t_max_ms <value>      - Sets the budget in milliseconds (overrides --t_max).\n");
	printf("  --i_rand <value>        - Sets the preferred i_rand.\n");
	printf("  --lower_bound <value>   - Sets the preferred lower_bound.\n");
	printf("  --seed <value>          - Sets the pseudo-random seed.\n");
	printf("  --initial_solution <f>  - Import initial solution from file.\n");
	printf("  --log_incumbent_solutions - Emit full incumbent routes as JSON lines.\n");
}

bool
str_eq(const char *str1, const char *str2)
{
	return str1 == str2 || (str1 && str2 && strcmp(str1, str2) == 0);
}

int
str_findlist(const char *value, unsigned count, const char** elements)
{
	for (unsigned i = 0; i < count; i++)
	{
		if (strcmp(value, elements[i]) == 0) return (int)i;
	}
	return -1;
}

static inline bool
at_end(void)
{
	return arg_index == arg_count - 1;
}

static inline const char *
next_arg(void)
{
	assert(!at_end());
	current_arg = args[++arg_index];
	return current_arg;
}

bool
match_longopt(const char *name)
{
	return str_eq(&current_arg[2], name);
}

static int
parse_multi_option(const char *start, unsigned count, const char **elements)
{
	const char *arg = current_arg;
	int select = str_findlist(start, count, elements);
	if (select < 0) panic("error: %.*s invalid option '%s' given.", (int)(start - arg), start, arg);
	return select;
}

static int
parse_next_int_value(const char *name)
{
	if (at_end())
		panic("error: --%s needs a valid integer.", name);

	const char *value_string = next_arg();
	char *p_end;
	errno = 0;
	long parsed = strtol(value_string, &p_end, 10);
	if (p_end == value_string || errno != 0 ||
	    p_end != value_string + strlen(value_string) ||
	    parsed < INT_MIN || parsed > INT_MAX) {
		panic("error: --%s needs a valid integer.", name);
	}
	return (int)parsed;
}

static uint64_t
parse_next_uint64_value(const char *name)
{
	if (at_end())
		panic("error: --%s needs a valid integer.", name);

	const char *value_string = next_arg();
	char *p_end;
	errno = 0;
	unsigned long long parsed = strtoull(value_string, &p_end, 10);
	if (p_end == value_string || errno != 0 ||
	    p_end != value_string + strlen(value_string)) {
		panic("error: --%s needs a valid integer.", name);
	}
	return (uint64_t)parsed;
}

static void
parse_option(void)
{
	switch (current_arg[1]) {
		case '-':
			if (match_longopt("beta_correction")) {
				options.beta_correction = true;
				return;
			}
			if (match_longopt("log_level")) {
				if (at_end())
					panic("error: --log_level needs a valid option.");
				options.log_level =
					(log_level) parse_multi_option(next_arg(), 3, log_levels);
				return;
			}
			if (match_longopt("n_near")) {
				options.n_near = parse_next_int_value("n_near");
				return;
			}
			if (match_longopt("k_max")) {
				options.k_max = parse_next_int_value("k_max");
				return;
			}
			if (match_longopt("t_max_ms")) {
				options.t_max_ms = (int64_t)parse_next_int_value("t_max_ms");
				options.has_t_max_ms = true;
				return;
			}
			if (match_longopt("t_max")) {
				options.t_max = (clock_t)parse_next_int_value("t_max");
				return;
			}
			if (match_longopt("initial_solution")) {
				if (at_end())
					panic("error: --initial_solution needs a file path.");
				options.initial_solution_file = next_arg();
				return;
			}
			if (match_longopt("log_incumbent_solutions")) {
				options.log_incumbent_solutions = true;
				return;
			}
			if (match_longopt("i_rand")) {
				options.i_rand = parse_next_int_value("i_rand");
				return;
			}
			if (match_longopt("lower_bound")) {
				options.lower_bound = parse_next_int_value("lower_bound");
				return;
			}
			if (match_longopt("seed")) {
				options.seed = parse_next_uint64_value("seed");
				options.has_seed = true;
				return;
			}
		default:
			break;
	}
	panic("Cannot process the unknown option \"%s\".", current_arg);
}

void
parse_arguments(int argc, const char *argv[])
{
	arg_count = argc;
	args = argv;

	if (argc < 3)
	{
		usage();
		exit(0);
	}

	options.problem_file = args[1];
	options.solution_file = args[2];
	options.initial_solution_file = NULL;
	options.log_incumbent_solutions = false;
	options.beta_correction = false;
	options.log_level = LOGLEVEL_VERBOSE;
	options.n_near = 100;
	options.k_max = 5;
	options.t_max = (clock_t)365 * 86400 * 100;
	options.t_max_ms = -1;
	options.has_t_max_ms = false;
	options.i_rand = 1000;
	options.lower_bound = 0;
	options.has_seed = false;
	options.seed = 0;

	for (arg_index = 3; arg_index < arg_count; arg_index++)
	{
		current_arg = args[arg_index];
		if (current_arg[0] == '-')
		{
			parse_option();
			continue;
		}
		panic("Found the unexpected argument \"%s\".", current_arg);
	}
}
