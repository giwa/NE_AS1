/*
 *	simsubnet.h
 */

#define	NNODE		64	/* max number of nodes per subnet */
#define	NINTF		8	/* max number of interfaces per node */
#define NDEST		64	/* max number of destination */
#define	DEFAULT_ROUTE	((uint32_t)0xffffffff)

struct subnet {
	struct subnet	*fp;		/* next entry */
	char		*name;		/* subnet name */
	uint32_t	prefix;		/* prefix */
	uint32_t	netmask;	/* netmask */
	int		masklen;	/* netmask length */
	struct node	*node_tab[NNODE];	/* node list */
};

struct node {
	struct node	*fp;		/* next entry in node list */
	char		*name;		/* node name */
	struct intf	*intf_tab[NINTF];	/* interface list */
	struct ftent	*fw_tab[NDEST];	/* forwarding table */
};

struct intf {
	uint32_t	ifaddr;		/* interface address */
	uint32_t	netmask;	/* netmask */
	int		masklen;	/* netmask length */
	struct subnet	*subnetp;	/* connected subnet */
};

struct ftent {
	uint32_t	destnet;	/* destination */
	uint32_t	netmask;	/* netmask */
	int		masklen;	/* netmask length */
	uint32_t	nextrt;		/* next hop router */
	struct intf	*intfp;		/* using interface */
};

/*
 *	global variables
 */
extern struct subnet *subnet_list_head;
extern struct subnet *subnet_list_tail;
extern struct node *node_list_head;
extern struct node *node_list_tail;

/*
 *	prototype declarations
 */
struct subnet *search_subnet(char *);
struct ftent *search_route(struct node *, uint32_t);
struct node *search_nodename_in_subnet(struct subnet *, char *);
struct node *search_node_in_subnet(struct subnet *, uint32_t);
struct node *search_node_in_list(char *name);
struct intf *search_intf_in_node(struct node *, uint32_t);
struct intf *search_intf_in_subnet(struct subnet *, uint32_t);
