/*
 *	In simsubnet: search.c
 *
 *	2013/02/18	ver. 0.1	by Fumio Teraoka
 *	2013/04/20	add default route
 */
#include <arpa/inet.h>
#include <stdio.h>      
#include <strings.h>
#include "simsubnet.h"

struct subnet *
search_subnet(char *name)
{
	struct subnet *sp;

	for (sp = subnet_list_head; sp; sp = sp->fp)
		if (strcmp(sp->name, name) == 0)
			return sp;
	return NULL;
}

struct ftent *
search_route(struct node *cnp, uint32_t daddr)
{
	struct ftent *rp, *maxrp = NULL;
	int i, maxlen = -1;

	for (i = 0; i < NDEST; i++) {
		if ((rp = cnp->fw_tab[i]) == NULL)
			break;
		if (rp->destnet == DEFAULT_ROUTE) {
			if (maxlen < 0) {
				maxlen = 0;
				maxrp = rp;
			}
		} else if ((daddr & htonl(rp->netmask)) == rp->destnet) {
			if (rp->masklen > maxlen) {
				maxlen = rp->masklen;
				maxrp = rp;
			}
		}
	}
	return maxrp;
}

struct node *
search_nodename_in_subnet(struct subnet *sp, char *name)
{
	struct node *np;
	int i;

	for (i = 0; i < NNODE; i++) {
		if ((np = sp->node_tab[i]) == NULL)
			return NULL;
		if (strcmp(np->name, name) == 0)
			return np;
	}
	return NULL;
}

struct node *
search_node_in_subnet(struct subnet *sp, uint32_t addr)
{
	struct node *np;
	struct intf *ip;
	int i, j;

	for (i = 0; i < NNODE; i++) {
		if ((np = sp->node_tab[i]) == NULL)
			return NULL;
		for (j = 0; j < NINTF; j++) {
			if ((ip = np->intf_tab[j]) == NULL)
				break;
			if (ip->ifaddr == addr)
				return np;
		}
	}
	return NULL;
}

struct node *
search_node_in_list(char *name)
{
	struct node *np;

	for (np = node_list_head; np; np = np->fp)
		if (strcmp(np->name, name) == 0)
			return np;
	return NULL;
}

struct intf *
search_intf_in_node(struct node *np, uint32_t addr)
{
	struct intf *ip;
	int i;

	for (i = 0; i < NINTF; i++) {
		if ((ip = np->intf_tab[i]) == NULL)
			return NULL;
		if (ip->ifaddr == addr)
			return ip;
	}
	return NULL;
}

struct intf *
search_intf_in_subnet(struct subnet *sp, uint32_t addr)
{
	struct node *np;
	struct intf *ip;
	int i;

	for (i = 0; i < NNODE; i++) {
		if ((np = sp->node_tab[i]) == NULL)
			break;
		if (ip = search_intf_in_node(np, addr))
			return ip;
	}
	return NULL;
}
