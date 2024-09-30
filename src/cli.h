#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_CLI_ARGS_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_CLI_ARGS_H

#include <stdbool.h>
#include <time.h>

typedef enum
{
    LOGLEVEL_NOT_SET = -1,
    LOGLEVEL_NONE,
    LOGLEVEL_NORMAL,
    LOGLEVEL_VERBOSE,
} log_level;

struct cli_options {
    const char *problem_file;
    const char *solution_file;
	bool beta_correction;
    log_level log_level;
    int n_near;
    int k_max;
    clock_t t_max;
    int i_rand;
    int lower_bound;
};

extern struct cli_options options;

void
parse_arguments(int argc, const char *argv[]);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_CLI_ARGS_H
