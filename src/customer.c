#include "customer.h"

//int customers_count = 0;

struct customer *
customer_dup(struct customer *c)
{
	//++customers_count;
	//printf("customers_count: %d\n", customers_count);
	//fflush(stdout);

	struct customer *dup = xmalloc(sizeof(*c)); //TODO: use mempool
	dup->id = c->id;
	dup->x = c->x;
	dup->y = c->y;
	dup->demand = c->demand;
	dup->e = c->e;
	dup->l = c->l;
	dup->s = c->s;
	dup->a = c->a;
	dup->tw_pf = c->tw_pf;
	dup->z = c->z;
	dup->tw_sf = c->tw_sf;
	dup->demand_pf = c->demand_pf;
	dup->demand_sf = c->demand_sf;
	dup->dist_pf = c->dist_pf;
	dup->dist_sf = c->dist_sf;
	dup->route = NULL;
	rlist_create(&dup->in_route);
	rlist_create(&dup->in_eject);
	rlist_create(&dup->in_eject_temp);
	rlist_create(&dup->in_opt_eject);
	return dup;
}

void
customer_delete(struct customer *c)
{
	//--customers_count;
	//printf("customers_count: %d\n", customers_count);
	//fflush(stdout);

	free(c); //TODO: return back to mempool
}
