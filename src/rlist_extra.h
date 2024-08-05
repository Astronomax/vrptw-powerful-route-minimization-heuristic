#ifndef EAMA_ROUTES_MINIMIZATION_HEURISTIC_RANDOM_RLIST_EXTRA_H
#define EAMA_ROUTES_MINIMIZATION_HEURISTIC_RANDOM_RLIST_EXTRA_H

#include "small/rlist.h"
#include "utils.h"

void ALWAYS_INLINE
rlist_swap_items(struct rlist *lhs, struct rlist *rhs)
{
	assert(lhs->next != lhs);
	assert(rhs->next != rhs);
	SWAP(lhs->prev, rhs->prev);
	SWAP(lhs->next, rhs->next);
	SWAP(lhs->prev->next, rhs->prev->next);
	SWAP(lhs->next->prev, rhs->next->prev);
}

void ALWAYS_INLINE
rlist_swap_tails(struct rlist *list1, struct rlist *list2,
	struct rlist *tail1, struct rlist *tail2)
{
	SWAP(tail1->next, tail2->next);
	SWAP(tail1->next->prev, tail2->next->prev);
	SWAP(list1->prev, list2->prev);
	SWAP(list1->prev->next, list2->prev->next);
}

#endif //EAMA_ROUTES_MINIMIZATION_HEURISTIC_RANDOM_RLIST_EXTRA_H
