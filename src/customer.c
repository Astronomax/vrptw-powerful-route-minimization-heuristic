#include "customer.h"

struct customer *
customer_dup(struct customer *c)
{
	/* TODO: use mempool to allocate customers */
	struct customer *dup = xmalloc(sizeof(*c));
	*dup = *c;
	dup->route = NULL;
	dup->idx = -1;
	rlist_create(&dup->in_route);
	return dup;
}

void
customer_delete(struct customer *c)
{
	/* TODO: return back to mempool */
	free(c);
}
