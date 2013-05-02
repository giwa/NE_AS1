/*
 *	In simsubnet: cmd_send.c
 *
 *	2013/02/18	ver. 0.1	By Fumio Teraoka
 */
#include <arpa/inet.h>
#include <stdio.h>      
#include "simsubnet.h"

/*
 *	send src_node dst_node
 */
void
cmd_send(int ac, char *av[])
{
	struct subnet *sp;
	struct node *snp, *cnp, *np;
	struct intf *ip;
	struct ftent *rp;
	struct in_addr in;
	uint32_t saddr = 0, daddr;
	int hop;

	if (ac != 3) {
		fprintf(stderr, "syntax error: %s\n", av[0]);
		return;
	}
	if ((snp = search_node_in_list(av[1])) == NULL) {
		fprintf(stderr, "no such source node: %s\n", av[1]);
		return;
	}
	if(inet_aton(av[2], (struct in_addr *)&daddr) == 0) {
		fprintf(stderr, "illegal destination addr: %s\n", av[2]);
		return;
	}
	if (search_intf_in_node(snp, daddr)) {
		fprintf(stderr, "source and destinatin are the same node\n");
		return;
	}

	/*
	 *	at source node
	 */
	hop = 0;
	in.s_addr = daddr;
	printf("%02d: send packet from node `%s' to addr `%s'\n",
		hop, snp->name, inet_ntoa(in));
	cnp = snp;

	for (;;) {
		/*
		 *	search routing table for next hop
		 */
		if ((rp = search_route(cnp, daddr)) == NULL) {
			in.s_addr = daddr;
			printf("no route to %s\n", inet_ntoa(in));
			return;
		}
		if (rp->nextrt == 0) {
			/*
			 *	dst on the same link
			 */
			printf("    found dst on the same link, ");
			in.s_addr = rp->intfp->ifaddr;
			printf("via %s\n", inet_ntoa(in));
			sp = rp->intfp->subnetp;
			if ((np = search_node_in_subnet(sp, daddr)) == NULL) {
				in.s_addr = daddr;
				printf("    No dst host: %s\n", inet_ntoa(in));
				return;
			}

			/*
			 *	at the source node, generate IP packet
			 */
			if (saddr == 0) {
				saddr = rp->intfp->ifaddr;
				in.s_addr = saddr;
				printf("    generated IP packet: src %s, ",
					inet_ntoa(in));
				in.s_addr = daddr;
				printf("dst %s\n", inet_ntoa(in));
			}

			/*
			 *	at the final destination node
			 */
			hop++;
			in.s_addr = daddr;
			printf("%02d: `%s' received packet via %s\n",
					hop, np->name, inet_ntoa(in));
			break;
		}

		/*
		 *	forwarding to next router
		 */
		sp = rp->intfp->subnetp;
		if ((np = search_node_in_subnet(sp, rp->nextrt)) == NULL) {
			printf("    cannot find next hop node\n");
			return;
		}
		in.s_addr = rp->nextrt;
		if (rp->destnet == DEFAULT_ROUTE)
			printf("    found default route: next router %s(`%s'), ",
				inet_ntoa(in), np->name);
		else
			printf("    found route: next router %s(`%s'), ",
				inet_ntoa(in), np->name);
		in.s_addr = rp->intfp->ifaddr;
		printf("via %s\n", inet_ntoa(in));

		/*
		 *	at the source node, generate IP packet
		 */
		if (saddr == 0) {
			saddr = rp->intfp->ifaddr;
			in.s_addr = saddr;
			printf("    generated IP packet: src %s, ",
				inet_ntoa(in));
			in.s_addr = daddr;
			printf("dst %s\n", inet_ntoa(in));
		}

		/*
		 *	packet forwarded to the next router
		 */
		cnp = np;
		hop++;
		in.s_addr = rp->nextrt;
		printf("%02d: `%s' received packet via %s\n",
			hop, np->name, inet_ntoa(in));

		if (search_intf_in_node(cnp, daddr)) {
			/*
			 *	dst is different intf on the node
			 */
			break;
		}
	}
	in.s_addr = daddr;
	printf("    packet reached final dst: %s\n", inet_ntoa(in));
}
