#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_MODIFICATION_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_MODIFICATION_H

#include "customer.h"

enum modification_type {
    TWO_OPT,
    OUT_RELOCATE,
    EXCHANGE,
    INSERT,
    EJECT,
    modification_count
};

struct modification {
    enum modification_type type;
    struct customer *v;
    /** Used only in TWO_OPT, OUT_RELOCATE, EXCHANGE, INSERT */
    struct customer *w;
};

int
modification_applicable(struct modification m);

void
modification_apply(struct modification m);

double
modification_delta(struct modification m, double alpha, double beta);

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_MODIFICATION_H
