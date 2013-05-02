/*
 *	simsubnet: subnet routing configuration simulator
 *
 *	% simsubnet <config_file>
 *
 *	2013/02/18	ver. 0.1	By Fumio Teraoka
 *	2013/04/20	bug fix (crash when only \n is entered)
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "simsubnet.h"

/* prototype declarations */
void config(FILE *);
static void cmd_help(int, char *[]);
static void cmd_quit(int, char *[]);
static void cmd_list(int, char *[]);
static void cmd_show(int, char *[]);
static void cmd_list_subnet(int, char *[]);
static void cmd_list_node(int, char *[]);
static void cmd_show_subnet(int, char *[]);
static void cmd_show_node(int, char *[]);
void cmd_send(int, char *[]);
static void print_subnet(struct subnet *);
static void print_node(struct node *, int, int);
static void getargs(char *, int *, char *[]);

struct cmd_tab_entry {
	char *cmd_name;
	void (*func)(int, char *[]);
} cmd_tab[] = {
	"help",		cmd_help,
	"quit",		cmd_quit,
	"bye",		cmd_quit,
	"exit",		cmd_quit,
	"list",		cmd_list,
	"show",		cmd_show,
	"send",		cmd_send,
	NULL,		NULL
};

#define	LBUFSIZE	256
#define	NARGS		16

main(int argc, char *argv[])
{
	FILE *fp;
	int i, ac;
	char *av[NARGS], lbuf[LBUFSIZE];
	struct cmd_tab_entry *cmdp;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
	}
	if ((fp = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}
	config(fp);
	printf("## configuration completed ##\n");

	for (;;) {
		fprintf(stderr, "sim$ ");
		if (fgets(lbuf, LBUFSIZE, stdin) == NULL) {
			if (feof(stdin))
				exit(0);	// exit if EOF
			perror("fgets");
			clearerr(stdin);	// clear error
			continue;
		}
		i = strlen(lbuf);
		lbuf[i-1] = '\0';		// remove '\n'
		getargs(lbuf, &ac, av);
		if (ac == 0)
			continue;
		for (cmdp = cmd_tab; cmdp->cmd_name; cmdp++) {
			if (strcmp(cmdp->cmd_name, av[0]) == 0) {
				(*cmdp->func)(ac, av);
				break;
			}
		}
		if (cmdp->cmd_name == NULL)
			fprintf(stderr, "no such command: %s\n", av[0]);
	}
}

static void
cmd_list(int ac, char *av[])
{
	if (ac < 2) {
		fprintf(stderr, "too few arguments\n");
		return;
	}
	if (strcmp(av[1], "subnet") == 0)
		cmd_list_subnet(ac, av);
	else if (strcmp(av[1], "node") == 0)
		cmd_list_node(ac, av);
	else
		fprintf(stderr, "no such command: %s %s\n", av[0], av[1]);
}

static void
cmd_show(int ac, char *av[])
{
	if (ac < 2) {
		fprintf(stderr, "too few arguments\n");
		return;
	}
	if (strcmp(av[1], "subnet") == 0)
		cmd_show_subnet(ac, av);
	else if (strcmp(av[1], "node") == 0)
		cmd_show_node(ac, av);
	else
		fprintf(stderr, "no such command: %s %s\n", av[0], av[1]);
}

static void
cmd_help(int ac, char *av[])
{
	printf("list subnet: show the list of subnets\n");
	printf("list node: show the list of nodes\n");
	printf("show subnet [<subnet_name>]: show subnet config\n");
	printf("show node [<node_name>]: show node config\n");
	printf("send from_node to_addr: send packet\n");
	printf("quit: quit simsubnet\n");
	printf("help: show help message\n");
}

static void
cmd_quit(int ac, char *av[])
{
	exit(0);
}

static void
cmd_list_subnet(int ac, char *av[])
{
	struct subnet *sp;
	struct in_addr in;

	for (sp = subnet_list_head; sp; sp = sp->fp) {
		in.s_addr = sp->prefix;
		printf("subnet %s (%s/%d)\n",
			sp->name, inet_ntoa(in), sp->masklen);
	}
}

static void
cmd_list_node(int ac, char *av[])
{
	struct node *np;

	for (np = node_list_head; np; np = np->fp)
		print_node(np, 0, 0);
}

static void
cmd_show_subnet(int ac, char *av[])
{
	struct subnet *sp;

	if (ac == 2) {
		for (sp = subnet_list_head; sp; sp = sp->fp) {
			print_subnet(sp);
			putchar('\n');
		}
	} else {
		for (sp = subnet_list_head; sp; sp = sp->fp) {
			if (strcmp(sp->name, av[2]) == 0) {
				print_subnet(sp);
				break;
			}
		}
		if (sp == NULL)
			printf("no such subnet: %s\n", av[2]);
	}
}

static void
cmd_show_node(int ac, char *av[])
{
	struct node *np;

	if (ac == 2) {
		for (np = node_list_head; np; np = np->fp)
			print_node(np, 0, 1);
	} else {
		for (np = node_list_head; np; np = np->fp) {
			if (strcmp(np->name, av[2]) == 0) {
				print_node(np, 0, 1);
				break;
			}
		}
		if (np == NULL)
			printf("no such node: %s\n", av[2]);
	}
}

static void
print_subnet(struct subnet *sp)
{
	struct node *np;
	struct in_addr in;
	int i;

	in.s_addr = sp->prefix;
//	printf("subnet %s (%s/0x%08x)\n", sp->name, inet_ntoa(in), sp->netmask);
	printf("subnet %s (%s/%d)\n", sp->name, inet_ntoa(in), sp->masklen);
	for (i = 0; i < NNODE; i++) {
		if ((np = sp->node_tab[i]) == NULL)
			break;
		print_node(np, 1, 1);
	}
}

void
print_node(struct node *np, int tabflag, int rtflag)
{
	int i;
	struct intf *ip;
	struct ftent *rp;
	struct in_addr in;

	if (tabflag)
		putchar('\t');
	printf("node %s ", np->name);
	for (i = 0; i < NINTF; i++) {
		if ((ip = np->intf_tab[i]) == NULL)
			break;
		in.s_addr = ip->ifaddr;
		printf("(%s/%d)", inet_ntoa(in), ip->masklen);
	}
	printf("\n");

	if (rtflag == 0)
		return;

	if (tabflag)
		putchar('\t');
	printf("    Destination\t\tNext Router\tInterface\n");
	for (i = 0; i < NDEST; i++) {
		if ((rp = np->fw_tab[i]) == NULL)
			break;
		if (tabflag)
			putchar('\t');
		if (rp->destnet == DEFAULT_ROUTE) {
			printf("    default\t");
		} else {
			in.s_addr = rp->destnet;
			printf("    %s/%d", inet_ntoa(in), rp->masklen);
			if (strlen(inet_ntoa(in)) < 10)
				putchar('\t');
		}
		in.s_addr = rp->nextrt;
		printf("\t%s", inet_ntoa(in));
		if (strlen(inet_ntoa(in)) < 8)
			putchar('\t');
		in.s_addr = rp->intfp->ifaddr;
		printf("\t%s\n", inet_ntoa(in));
	}
}

static void
getargs(char *cp, int *argc, char **argv)
{

        *argc = 0;
        argv[*argc] = NULL;

loop:
        while (*cp && isblank(*cp))     // skip space and tab
                cp++;

        if (*cp == '\0')
                return;

        argv[(*argc)++] = cp;

        while (*cp && !isblank(*cp))    // search for end of word
                cp++;
        if (*cp == '\0')
                return;

        *cp++ = '\0';
        goto loop;
}

