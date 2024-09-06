#ifndef TARANTOOL_RLIST_PERSISTENT_H_INCLUDED
#define TARANTOOL_RLIST_PERSISTENT_H_INCLUDED
/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stddef.h>
#include "small/rlist.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#ifndef typeof
/* TODO: 'typeof' is a GNU extension */
#define typeof __typeof__
#endif

/** list of rlist_assign_op's */
typedef struct rlist rlist_persistent_history;

/**
 * rlist_persistent savepoint. It's possible to save the current
 * rlist_persistent state in a savepoint and roll back to the saved state at
 * any time
 */
typedef struct rlist_assign_op * rlist_persistent_svp;

struct rlist_assign_op {
    struct rlist **var;
    struct rlist *prev_val, *next_val;
    struct rlist in_history;
};

static SMALL_ALWAYS_INLINE struct rlist_assign_op *
rlist_assign_op_new(struct rlist **var, struct rlist *prev_val,
	struct rlist *next_val)
{
	struct rlist_assign_op *op = malloc(sizeof(*op)); //TODO: use mempool
	op->var = var;
	op->prev_val = prev_val;
	op->next_val = next_val;
	rlist_create(&op->in_history);
	return op;
}

static SMALL_ALWAYS_INLINE void
rlist_assign_op_delete(struct rlist_assign_op *op)
{
	free(op); //TODO: return back to mempool
}

static SMALL_ALWAYS_INLINE rlist_persistent_svp
rlist_persistent_create_svp(rlist_persistent_history *history)
{
	return rlist_last_entry(history, struct rlist_assign_op, in_history);
}

/** Forget suffix of operations after the savepoint. */
static SMALL_ALWAYS_INLINE void
rlist_persistent_rollback_to_svp(rlist_persistent_history *history,
				 rlist_persistent_svp svp) {
	struct rlist_assign_op *op, *tmp;
	rlist_foreach_entry_safe_reverse(op, history, in_history, tmp) {
		if (op == svp)
			return;
		assert(*op->var == op->next_val);
		*op->var = op->prev_val;
		rlist_del_entry(op, in_history);
		rlist_assign_op_delete(op);
	}
	assert(history == &svp->in_history);
}

#define rlist_assign(x, y) do {						\
	struct rlist_assign_op *op = rlist_assign_op_new(&x, x, y); 	\
	rlist_add_tail_entry(history, op, in_history);                 	\
	x = y;                         					\
} while(0)

//void rlist_assign(struct rlist **x, struct rlist *y) {
//	struct rlist_assign_op *op = rlist_assign_op_new(x, *x, y);
//	rlist_add_tail_entry(history, op, in_history);
//	*x = y;
//}

/**
 * init list head (or list entry as ins't included in list)
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_create(rlist_persistent_history *history,
			struct rlist *node)
{
	rlist_assign(node->next, node);
	rlist_assign(node->prev, node);
}

/**
 * add item to list
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_add(rlist_persistent_history *history,
		     struct rlist *head, struct rlist *item)
{
	rlist_assign(item->prev, head);
	rlist_assign(item->next, head->next);
	rlist_assign(item->prev->next, item);
	rlist_assign(item->next->prev, item);
}

/**
 * add item to list tail
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_add_tail(rlist_persistent_history *history,
			  struct rlist *head, struct rlist *item)
{
	rlist_assign(item->next, head);
	rlist_assign(item->prev, head->prev);
	rlist_assign(item->prev->next, item);
	rlist_assign(item->next->prev, item);
}

/**
 * delete element
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_del(rlist_persistent_history *history,
		     struct rlist *item)
{
	rlist_assign(item->prev->next, item->next);
	rlist_assign(item->next->prev, item->prev);
	rlist_persistent_create(history, item);
}

static SMALL_ALWAYS_INLINE struct rlist *
rlist_persistent_shift(rlist_persistent_history *history,
		       struct rlist *head)
{
	struct rlist *shift = head->next;
	rlist_assign(head->next, shift->next);
	rlist_assign(shift->next->prev, head);
	rlist_assign(shift->next, shift);
	rlist_assign(shift->prev, shift);
	return shift;
}

static SMALL_ALWAYS_INLINE struct rlist *
rlist_persistent_shift_tail(rlist_persistent_history *history,
			    struct rlist *head)
{
	struct rlist *shift = head->prev;
	rlist_persistent_del(history, shift);
	return shift;
}

/**
 * return first element
 */
#define rlist_persistent_first(head) rlist_first(head)

/**
 * return last element
 */
#define rlist_persistent_last(head) rlist_last(head)

/**
 * return next element by element
 */
#define rlist_persistent_next(item) rlist_next(item)

/**
 * return previous element
 */
#define rlist_persistent_prev(item) rlist_prev(item)

/**
 * return TRUE if list is empty
 */
#define rlist_persistent_empty(head) rlist_empty(head)

/**
@brief delete from one list and add as another's head
@param to the head that will precede our entry
@param item the entry to move
*/
static SMALL_ALWAYS_INLINE void
rlist_persistent_move(rlist_persistent_history *history,
		      struct rlist *to, struct rlist *item)
{
	rlist_persistent_del(history, item);
	rlist_persistent_add(history, to, item);
}

/**
@brief delete from one list and add_tail as another's head
@param to the head that will precede our entry
@param item the entry to move
*/
static SMALL_ALWAYS_INLINE void
rlist_persistent_move_tail(rlist_persistent_history *history,
			   struct rlist *to, struct rlist *item)
{
	rlist_assign(item->prev->next, item->next);
	rlist_assign(item->next->prev, item->prev);
	rlist_assign(item->next, to);
	rlist_assign(item->prev, to->prev);
	rlist_assign(item->prev->next, item);
	rlist_assign(item->next->prev, item);
}

static SMALL_ALWAYS_INLINE void
rlist_persistent_swap_links(rlist_persistent_history *history,
			    struct rlist *rhs, struct rlist *lhs)
{
	struct rlist *tmp_prev = rhs->prev;
	struct rlist *tmp_next = rhs->next;
	/* Relink the nodes. */
	if (lhs->next == lhs) {             /* Take care of empty list case */
		rlist_assign(rhs->prev, rhs);
		rlist_assign(rhs->next, rhs);
	} else {
		rlist_assign(rhs->prev, lhs->prev);
		rlist_assign(rhs->next, lhs->next);
	}
	if (tmp_next == rhs) {             /* Take care of empty list case */
		rlist_assign(lhs->prev, lhs);
		rlist_assign(lhs->next, lhs);
	} else {
		rlist_assign(lhs->prev, tmp_prev);
		rlist_assign(lhs->next, tmp_next);
	}
}

/**
 * move all items of list head2 to the head of list head1
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_splice(rlist_persistent_history *history,
			struct rlist *head1, struct rlist *head2)
{
	if (!rlist_persistent_empty(head2)) {
		rlist_assign(head1->next->prev, head2->prev);
		rlist_assign(head2->prev->next, head1->next);
		rlist_assign(head1->next, head2->next);
		rlist_assign(head2->next->prev, head1);
		rlist_persistent_create(history, head2);
	}
}

/**
 * move all items of list head2 to the tail of list head1
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_splice_tail(rlist_persistent_history *history,
			     struct rlist *head1, struct rlist *head2)
{
	if (!rlist_persistent_empty(head2)) {
		rlist_assign(head1->prev->next, head2->next);
		rlist_assign(head2->next->prev, head1->prev);
		rlist_assign(head1->prev, head2->prev);
		rlist_assign(head2->prev->next, head1);
		rlist_persistent_create(history, head2);
	}
}

/**
 * move the initial part of list head2, up to but excluding item,
 * to list head1; the old content of head1 is discarded
 */
static SMALL_ALWAYS_INLINE void
rlist_persistent_cut_before(rlist_persistent_history *history,
			    struct rlist *head1, struct rlist *head2,
			    struct rlist *item)
{
	if (head1->next == item) {
		rlist_persistent_create(history, head1);
		return;
	}
	rlist_assign(head1->next, head2->next);
	rlist_assign(head1->next->prev, head1);
	rlist_assign(head1->prev, item->prev);
	rlist_assign(head1->prev->next, head1);
	rlist_assign(head2->next, item);
	rlist_assign(item->prev, head2);
}

/**
 * return entry by list item
 */
#define rlist_persistent_entry(item, type, member)				\
	rlist_entry(item, type, member)

/**
 * return first entry
 */
#define rlist_persistent_first_entry(head, type, member)			\
        rlist_first_entry(head, type, member)

/**
 * Remove one element from the list and return it
 * @pre the list is not empty
 */
#define rlist_persistent_shift_entry(history, head, type, member)			\
        rlist_persistent_entry(rlist_persistent_shift(history, head), type, member)	\

/**
 * Remove one element from the list tail and return it
 * @pre the list is not empty
 */
#define rlist_persistent_shift_tail_entry(history, head, type, member)				\
        rlist_persistent_entry(rlist_persistent_shift_tail(history, head), type, member)	\

/**
 * return last entry
 * @pre the list is not empty
 */
#define rlist_persistent_last_entry(head, type, member)				\
        rlist_last_entry(head, type, member)


/**
 * return next entry
 */
#define rlist_persistent_next_entry(item, member)				\
        rlist_next_entry(item, member)

/**
 * return previous entry
 */
#define rlist_persistent_prev_entry(item, member)				\
	rlist_prev_entry(item, member)

#define rlist_persistent_prev_entry_safe(item, head, member)			\
	rlist_prev_entry_safe(item, head, member)

/**
 * add entry to list
 */
#define rlist_persistent_add_entry(history, head, item, member)			\
	rlist_persistent_add(history, (head), &(item)->member)

/**
 * add entry to list tail
 */
#define rlist_persistent_add_tail_entry(history, head, item, member)		\
	rlist_persistent_add_tail(history, (head), &(item)->member)

/**
delete from one list and add as another's head
*/
#define rlist_persistent_move_entry(history, to, item, member)			\
	rlist_persistent_move(history, (to), &((item)->member))

/**
delete from one list and add_tail as another's head
*/
#define rlist_persistent_move_tail_entry(history, to, item, member)		\
	rlist_persistent_move_tail(history, (to), &((item)->member))

/**
 * delete entry from list
 */
#define rlist_persistent_del_entry(history, item, member)			\
	rlist_persistent_del(history, &((item)->member))

/**
 * foreach through list
 */
#define rlist_persistent_foreach(item, head)					\
        rlist_foreach(item, head)

/**
 * foreach backward through list
 */
#define rlist_persistent_foreach_reverse(item, head)				\
        rlist_foreach_reverse(item, head)

/**
 * return true if entry points to head of list
 *
 * NOTE: avoid using &item->member, because it may result in ASAN errors
 * in case the item type or member is supposed to be aligned, and the item
 * points to the list head.
 */
#define rlist_persistent_entry_is_head(item, head, member)			\
        rlist_entry_is_head(item, head, member)

/**
 * foreach through all list entries
 */
#define rlist_persistent_foreach_entry(item, head, member)			\
        rlist_foreach_entry(item, head, member)

/**
 * foreach backward through all list entries
 */
#define rlist_persistent_foreach_entry_reverse(item, head, member)		\
        rlist_foreach_entry_reverse(item, head, member)

/**
 * foreach through all list entries safe against removal
 */
#define rlist_persistent_foreach_entry_safe(item, head, member, tmp)		\
	rlist_foreach_entry_safe(item, head, member, tmp)

/**
 * foreach backward through all list entries safe against removal
 */
#define rlist_persistent_foreach_entry_safe_reverse(item, head, member, tmp)	\
	rlist_foreach_entry_safe_reverse(item, head, member, tmp)

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* TARANTOOL_RLIST_PERSISTENT_H_INCLUDED */
