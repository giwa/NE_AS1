/*
 *	In simsubnet: config.c
 *
 *	2013/02/18	ver 0.1		By Fumio Teraoka
 *	2013/04/29	bug fix		"_" can be used as label.
 *					check illegal character.
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include "simsubnet.h"

/*      
 *      token type
 */     
#define TKN_EOF		-1	/* end of file */
#define TKN_NONE	0	/* no characters */
#define TKN_SUBNET	1	/* keyword "subnet" */
#define TKN_NODE	2	/* keyword "node" */
#define	TKN_PREFIX	3	/* keyword "prefix" */
#define	TKN_FWTAB	4	/* keyword "fwtab" */
#define TKN_LBRACE	5	/* left brace `{' */
#define TKN_RBRACE	6	/* right brace `}' */
#define TKN_TERM	7	/* terminator `;' */
#define	TKN_HYPHEN	8	/* hyphen `-' */
#define TKN_LABEL	9	/* label (alpha-numeric) */
#define TKN_ADDR	10	/* address (including CIDR format) */

#define MAXTOKENLEN	64	/* token max length */

#define isdelimiter_tkn(x)\
	 ((x) == TKN_LBRACE || (x) == TKN_RBRACE || (x) == TKN_TERM)

/* global variables */
struct subnet *subnet_list_head = NULL;
struct subnet *subnet_list_tail = NULL;
struct node *node_list_head = NULL;
struct node *node_list_tail = NULL;

static char deli_chars[] = {'{', '}', ';', '\0'};
static int deli_tkns[] = {TKN_LBRACE, TKN_RBRACE, TKN_TERM};

static struct deli_chars {
	char	deli_char;
	int	tkn_type;
} deli_chartab[] = {
	'{',	TKN_LBRACE,
	'}',	TKN_RBRACE,
	';',	TKN_TERM,
	'-',	TKN_HYPHEN,
	'\0',	0
};

static struct keywords {
	char	*tkn_word;;
	int	tkn_type;
} keyword_tab[] = {
	"subnet",	TKN_SUBNET,
	"node",		TKN_NODE,
	"prefix",	TKN_PREFIX,
	"fwtab",	TKN_FWTAB,
	NULL,		0
};

/* prototype declaration */
static int gettoken(FILE *, char *, int, int *);
static int isdelimiter(char);
static int classify_token(char *, int);
static int iskeyword(char *);
static void config_subnet(FILE *);
static void config_prefix(struct subnet *, FILE *);
static void config_node(struct subnet *, FILE *);
static void config_intf(struct node *, struct subnet *, FILE *);
static void config_fwtab(FILE *);
static void config_ftent(struct node *, char *, FILE *, int);

#ifdef DEBUG0
static char *tkntype_to_str(int);
static char *stat_to_str(int);
#endif

void
config(FILE *fp)
{
	char token[MAXTOKENLEN];
	int tkn_type, lnum;
	struct node *np;

	for (;;) {
		tkn_type = gettoken(fp, token, MAXTOKENLEN, &lnum);
		if (tkn_type == TKN_EOF)
			break;
		if (tkn_type == TKN_NONE)
			continue;
		if (tkn_type == TKN_SUBNET) {
			config_subnet(fp);
		} else if (tkn_type == TKN_FWTAB) {
			config_fwtab(fp);
		} else {
			fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
			exit(1);
		}
	}
	return;
}

static void
config_subnet(FILE *fp)
{
	struct subnet *sp;
	int tkn_type, lnum;
	char token[MAXTOKENLEN];

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_LABEL) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	for (sp = subnet_list_head; sp; sp = sp->fp) {
		if (strcmp(sp->name, token) == 0) {
			fprintf(stderr, "%d: subnet `%s' already defined\n",
				lnum, token);
			exit(1);
		}
	}

	/* allocate subnet */
	if ((sp = (struct subnet *)malloc(sizeof(struct subnet))) == NULL) {
		perror("malloc");
		exit(1);
	}
	bzero(sp, sizeof(struct subnet));

	/* added the new subnet to the tail of the subnet list */
	if (subnet_list_head == NULL) {
		subnet_list_head = subnet_list_tail = sp;
	} else {
		subnet_list_tail->fp = sp;
		subnet_list_tail = sp;
	}

	/* store subnet name */
	if ((sp->name = (char *)malloc(strlen(token)+1)) == NULL) {
		perror("malloc");
		exit(1);
	}
	strcpy(sp->name, token);
	
	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_LBRACE) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}

	/* configure prefix */
	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_PREFIX) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	config_prefix(sp, fp);

	for (;;) {
		tkn_type = gettoken(fp, token, MAXTOKENLEN, &lnum);
		if (tkn_type == TKN_RBRACE)
			break;
		if (tkn_type != TKN_NODE) {
			fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
			exit(1);
		}
		config_node(sp, fp);
	}

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_TERM) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
}

static void
config_prefix(struct subnet *sp, FILE *fp)
{
	char *pp, *mp, token[MAXTOKENLEN];
	int masklen, i, lnum;
	uint32_t mask;

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_ADDR) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	pp = token;
	if ((mp = strchr(token, '/')) == NULL) {
		fprintf(stderr, "%d: illegal prefix: %s\n", lnum, token);
		exit(1);
	}
	*mp++ = '\0';

	if (inet_aton(pp, (struct in_addr *)&(sp->prefix)) == 0) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	masklen = strtol(mp, NULL, 10);
	if (masklen < 1 || masklen > 30) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	sp->masklen = masklen;		/* store netmask length */

	mask = 0x80000000;
	for (i = 0; i < masklen-1; i++)
		mask = 0x80000000 | (mask >> 1);
	sp->netmask = mask;

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_TERM) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
}
	
static void
config_node(struct subnet *sp, FILE *fp)
{
	struct node *np;
	char token[MAXTOKENLEN];
	int lnum, i;

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_LABEL) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	if (search_nodename_in_subnet(sp, token)) {
		fprintf(stderr,
			"%d: node `%s' already connected to subnet `%s'\n",
			lnum, np->name, sp->name);
		exit(1);
	}

	/* search node_list for specified node */
	if ((np = search_node_in_list(token)) == NULL) {
		/* new node is created */
		if ((np = (struct node *)malloc(sizeof(struct node)))
				== NULL) {
			perror("malloc");
			exit(1);
		}
		bzero(np, sizeof(struct node));
		if ((np->name = (char *)malloc(strlen(token)+1)) == NULL) {
			perror("malloc");
			exit(1);
		}
		strcpy(np->name, token);	/* store node name */
		if (node_list_head == NULL) {
			node_list_head = node_list_tail = np;
		} else {
			node_list_tail->fp = np;
			node_list_tail = np;
		}
	}

	/* search for the tail of the subnet node table */
	for (i = 0; i < NNODE; i++)
		if (sp->node_tab[i] == NULL)
			break;
	if (i == NNODE) {
		fprintf(stderr, "too many nodes in a subnet: %s\n", sp->name);
		exit(1);
	}
	sp->node_tab[i] = np;

	config_intf(np, sp, fp);
}

static void
config_intf(struct node *np, struct subnet *sp, FILE *fp)
{
	struct intf *ip;
	char token[MAXTOKENLEN];
	int lnum, i;
	uint32_t addr;

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_ADDR) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	if (inet_aton(token, (struct in_addr *)&(addr)) == 0) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	if (search_intf_in_subnet(sp, addr)) {
		fprintf(stderr, "%d: duplicate addr %s in subnet %s\n",
			lnum, token, sp->name);
		exit(1);
	}
	if (sp->prefix != (addr & htonl(sp->netmask))) {
		fprintf(stderr, "%d: illegal address: %s\n", lnum, token);
		exit(1);
	}

	if ((ip = (struct intf *)malloc(sizeof(struct intf))) == NULL) {
		perror("malloc");
		exit(1);
	}
	bzero(ip, sizeof(struct intf));

	/* search for the tail of the intf list in the node */
	for (i = 0; i < NINTF; i++)
		if (np->intf_tab[i] == NULL)
			break;
	if (i == NINTF) {
		fprintf(stderr, "too many interfaces in a node: %s\n",
				np->name);
		exit(1);
	}
	np->intf_tab[i] = ip;

	ip->ifaddr = addr;		/* store interface address */
	ip->netmask = sp->netmask;	/* store netmask */
	ip->masklen = sp->masklen;	/* store netmask length */
	ip->subnetp = sp;		/* store pointer to subnet */

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_TERM) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
}

static void
config_fwtab(FILE *fp)
{
	struct node *np;
	char token[MAXTOKENLEN];
	int lnum, tkn_type;
#ifdef DEBUG0
	struct ftent *rp;
	struct in_addr in1, in2;
	int i;
#endif

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_LABEL) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}

	/* search for the node in the node list */
	if ((np = search_node_in_list(token)) == NULL) {
		fprintf(stderr, "%d: no such node: %s\n", lnum, token);
		exit(1);
	}

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_LBRACE) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}

	for (;;) {
		tkn_type= gettoken(fp, token, MAXTOKENLEN, &lnum);
		if (tkn_type == TKN_RBRACE)
			break;
		if (tkn_type != TKN_ADDR) {
			fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
			exit(1);
		}
		config_ftent(np, token, fp, lnum);
	}

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_TERM) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
#ifdef DEBUG0
	fprintf(stderr, "config_fwtab: node %s\n", np->name);
	for (i = 0; i < NDEST; i++) {
		if ((rp = np->fw_tab[i]) == NULL)
			break;
		in1.s_addr = rp->destnet;
		in2.s_addr = rp->nextrt;
		fprintf(stderr, "  %s/0x%08x -> ", inet_ntoa(in1), rp->netmask);
		fprintf(stderr, "%s\n", inet_ntoa(in2));
	}
#endif
}

static void
config_ftent(struct node *np, char *addr, FILE *fp, int lnum)
{
	struct ftent *rp;
	struct intf *ip;
	char *ap, *mp = NULL, token[MAXTOKENLEN];
	int i, masklen, tkn_type;
	uint32_t mask, ifaddr;

	/* allocate rourint entry */
	if ((rp = (struct ftent *)malloc(sizeof(struct ftent))) == NULL) {
		perror("malloc");
		exit(1);
	}
	bzero(rp, sizeof(struct ftent));

	/* search for empty slot in forwarding table */
	for (i = 0; i < NDEST; i++)
		if (np->fw_tab[i] == NULL)
			break;
	if (i == NDEST) {
		fprintf(stderr, "%d: too many routining entry\n", lnum);
		exit(1);
	}
	np->fw_tab[i] = rp;

	ap = addr;
	if (mp = strchr(addr, '/')) {
		*mp++ = '\0';

		masklen = strtol(mp, NULL, 10);
		if (masklen < 1 || masklen > 32) {
			fprintf(stderr, "%d: illegal netmask: %s\n", lnum, mp);
			exit(1);
		}
		rp->masklen = masklen;		/* store netmask length */

		mask = 0x80000000;
		for (i = 0; i < masklen-1; i++) {
			mask = 0x80000000 | (mask >> 1);
		}
		rp->netmask = mask;	/* store netmask */
	}

	/* store destination network address */
	if (inet_aton(ap, (struct in_addr *)&(rp->destnet)) == 0) {
		fprintf(stderr, "%d: illegal address: %s\n", lnum, ap);
		exit(1);
	}

	tkn_type= gettoken(fp, token, MAXTOKENLEN, &lnum);
	if (tkn_type == TKN_ADDR) {
		/* store next router */
		if (inet_aton(token, (struct in_addr *)&(rp->nextrt)) == 0) {
			fprintf(stderr, "%d: illegal address: %s\n",
					lnum, token);
			exit(1);
		}
	} else if (tkn_type != TKN_HYPHEN) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_ADDR) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
	if (inet_aton(token, (struct in_addr *)&ifaddr) == 0) {
		fprintf(stderr, "%d: illegal address: %s\n", lnum, token);
		exit(1);
	}

	/* search for intf in the node */
	if ((ip = search_intf_in_node(np, ifaddr)) == NULL) {
		fprintf(stderr, "%d: no such interface: %s\n", lnum, token);
		exit(1);
	}
	rp->intfp = ip;			/* store pointer to intf */

	if (gettoken(fp, token, MAXTOKENLEN, &lnum) != TKN_TERM) {
		fprintf(stderr, "%d: syntax error: %s\n", lnum, token);
		exit(1);
	}
}

static int
gettoken(FILE *fp, char *token, int len, int *lnum)
{
	static int linenum = 1;			/* line number */
	int c, tkn_type;
	char *p = token;

	*lnum = linenum;
	*p = '\0';

	/* skip space chars and new-line */
	while (isblank(c = getc(fp)) || c == '\n' || c == '%') {
		if (c == '%') {
			while ((c = getc(fp)) != '\n')
				;
		}
		if (c == '\n')
			*lnum = ++linenum;
	}
	if (c == EOF)
		return TKN_EOF;

	/* is delimiter? */
	if ((tkn_type = isdelimiter(c)) > 0)
		return tkn_type;

	do {
		*p++ = c;
		c = getc(fp);
		if (!isalnum(c) && c != '_' && c != '.' && c != '/' &&
					!isdelimiter(c) && !isspace(c)) {
			fprintf(stderr, "%d: illegal character: `%c'\n",
					linenum, c);
			exit(1);
		}
	} while (c != EOF && (tkn_type = isdelimiter(c)) <= 0 && !isspace(c)
							&& c != '\n');
	*p = '\0';

	/* token ends with delimiter, then ungetc() */
	if (c != EOF && isdelimiter(c) > 0)
		ungetc(c, fp);

	/* token ends with new-line */
	if (c == '\n')
		*lnum = ++linenum;

	return classify_token(token, linenum);
}

static int
isdelimiter(char c)
{
	struct deli_chars *p;

	for (p = deli_chartab; p->deli_char; p++)
		if (c == p->deli_char)
			return p->tkn_type;
	return TKN_NONE;
}

static int
classify_token(char *token, int lnum)
{
	char *p = token;
	char buf[32], *q;
	int dotcnt, slashcnt, tmpnum, slashflg, addrflg = 0;
	int tkn_type;

	if ((tkn_type = iskeyword(token)) > 0)	/* keyword ? */
		return tkn_type;

	if (*p == '-')
		return TKN_HYPHEN;

	if (isalpha(*p) || *p == '_') {	/* alnum (lavel) ? */
		while (isalnum(*p) || *p == '_')
			p++;
		if (*p == '\0') {
			return TKN_LABEL;
		} else {
			fprintf(stderr, "%d: illegal char: %c\n", lnum, *p);
			exit(1);
		}
	}
	if (isdigit(*p)) {	/* address (including CIDR format) ? */
		dotcnt = slashcnt = slashflg = 0;
		do {
			tmpnum = strtol(p, &q, 10);	/* get a number */
			if (slashflg) {
				/* after '/', number is mask */
				if (tmpnum < 0 || tmpnum > 32) {
					fprintf(stderr,
					"%d: illegal netmask: %s\n", lnum, p);
					exit(1);
				}
				slashflg = 0;
			} else {
				/* a part of IP address */
				if (tmpnum < 0 || tmpnum > 255) {
					fprintf(stderr,
					"%d: illegal address: %s\n", lnum, p);
					exit(1);
				}
				addrflg++;	/* # of address part */
			}
			if (*q == '.')
				dotcnt++;
			else if (*q == '/') {
				slashcnt++;
				slashflg++;
			}
			p = q + 1;
		} while (*q != '\0');
		if (dotcnt > 4) {
			fprintf(stderr,
				"%d: illegal address: %s\n", lnum, token);
			exit(1);
		}
		if (slashcnt > 1) {
			fprintf(stderr,
				"%d: illegal address: %s\n", lnum, token);
			exit(1);
		}
		if (slashcnt == 1 && addrflg == 0) {
			fprintf(stderr,
				"%d: illegal address: %s\n", lnum, token);
			exit(1);
		}
		return TKN_ADDR;
	}
}

static int
iskeyword(char *token)
{
	struct keywords *p;

	for (p = keyword_tab; p->tkn_word; p++)
		if (strcmp(p->tkn_word, token) == 0)
			return p->tkn_type;
	return -1;
}

#ifdef DEBUG0
static struct tokens {
	int	tkn_type;
	char	*tkn_name;
} tkn_tab[] = {
	TKN_EOF,	"TKN_EOF",
	TKN_NONE,	"TKN_NONE",
	TKN_SUBNET,	"TKN_SUBNET",
	TKN_NODE,	"TKN_NODE",
	TKN_PREFIX,	"TKN_PREFIX",
	TKN_FWTAB,	"TKN_FWTAB",
	TKN_LBRACE,	"TKN_LBRACE",
	TKN_RBRACE,	"TKN_RBRACE",
	TKN_TERM,	"TKN_TERM",
	TKN_HYPHEN,	"TKN_HYPHEN",
	TKN_LABEL,	"TKN_LABEL",
	TKN_ADDR,	"TKN_ADDR",
	0,		NULL
};

static struct states {
	int	stat_type;
	char	*stat_name;
} state_tab[] = {
	0,	"STAT_IDLE",
	1,	"STAT_IN_SUBNET",
	2,	"STAT_IN_SUBNET_GOT_NAME",
	3,	"STAT_IN_SUBNET_GOT_LBRACE",
	4,	"STAT_IN_SUBNET_IN_PREFIX",
	5,	"STAT_IN_SUBNET_IN_PREFIX_GOT_ADDR",
	6,	"STAT_IN_SUBNET_IN_NODE",
	7,	"STAT_IN_SUBNET_IN_NODE_GOT_NAME",
	8,	"STAT_IN_SUBNET_IN_NODE_GOT_ADDR",
	9,	"STAT_IN_SUBNET_GOT_RBRACE",
	10,	"STAT_IN_FWTAB",
	11,	"STAT_IN_FWTAB_GOT_NAME",
	12,	"STAT_IN_FWTAB_GOT_LBRACE",
	13,	"STAT_IN_FWTAB_GOT_ADDR",
	14,	"STAT_IN_FWTAB_GOT_NEXT",
	15,	"STAT_IN_FWTAB_GOT_RBRACE",
	-1,	NULL
};

static char *
tkntype_to_str(int tkn_type)
{
	struct tokens *p;

	for (p = tkn_tab; p->tkn_name; p++)
		if (p->tkn_type == tkn_type)
			return p->tkn_name;
	fprintf(stderr, "tkntype_to_str: unknown token type!: %d\n", tkn_type);
	exit(1);
}

static char *
stat_to_str(int stat)
{
	struct states *p;

	for (p = state_tab; p->stat_type >= 0; p++)
		if (p->stat_type == stat)
			return p->stat_name;
	fprintf(stderr, "stat_to_str: unknown state!: %d\n", stat);
	exit(1);
}
#endif	/* DEBUG */
