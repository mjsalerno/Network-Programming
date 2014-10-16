#include	"unpifiplus.h"
#include <net/if.h>

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);
char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen);


int
main(int argc, char **argv)
{
	struct ifi_info	*ifi, *ifihead;
	struct sockaddr	*sa;
	u_char		*ptr;
	int		i, family, doaliases;

	if (argc != 3) {
            printf("usage: prifinfo_plus <inet4|inet6> <doaliases>\n");
            exit(EXIT_FAILURE);
        }

	if (strcmp(argv[1], "inet4") == 0)
		family = AF_INET;
#ifdef	IPv6
	else if (strcmp(argv[1], "inet6") == 0)
		family = AF_INET6;
#endif
	else{
		fprintf(stderr, "invalid <address-family>\n");
		exit(EXIT_FAILURE);
	}
	doaliases = atoi(argv[2]);
	ifihead = ifi = get_ifi_info_plus(family, doaliases);
	if(ifihead == NULL || ifi == NULL){
		fprintf(stderr, "get_ifi_info_plus error: no interfaces found\n");
		exit(EXIT_FAILURE);
	}

	for (; ifi != NULL; ifi = ifi->ifi_next) {
		printf("%s: ", ifi->ifi_name);
		if (ifi->ifi_index != 0)
			printf("(%d) ", ifi->ifi_index);
		printf("<");
/* *INDENT-OFF* */
		if (ifi->ifi_flags & IFF_UP)			printf("UP ");
		if (ifi->ifi_flags & IFF_BROADCAST)		printf("BCAST ");
		if (ifi->ifi_flags & IFF_MULTICAST)		printf("MCAST ");
		if (ifi->ifi_flags & IFF_LOOPBACK)		printf("LOOP ");
		if (ifi->ifi_flags & IFF_POINTOPOINT)	printf("P2P ");
		printf(">\n");
/* *INDENT-ON* */

		if ( (i = ifi->ifi_hlen) > 0) {
			ptr = ifi->ifi_haddr;
			do {
				printf("%s%x", (i == ifi->ifi_hlen) ? "  " : ":", *ptr++);
			} while (--i > 0);
			printf("\n");
		}
		if (ifi->ifi_mtu != 0)
			printf("  MTU: %d\n", ifi->ifi_mtu);

		if ( (sa = ifi->ifi_addr) != NULL){
			printf("  IP addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));
		}

/*=================== cse 533 Assignment 2 modifications ======================*/

		if ( (sa = ifi->ifi_ntmaddr) != NULL)
			printf("  network mask: %s\n",
						sock_ntop_host(sa, sizeof(*sa)));

/*=============================================================================*/

		if ( (sa = ifi->ifi_brdaddr) != NULL)
			printf("  broadcast addr: %s\n",
						sock_ntop_host(sa, sizeof(*sa)));
		if ( (sa = ifi->ifi_dstaddr) != NULL)
			printf("  destination addr: %s\n",
						sock_ntop_host(sa, sizeof(*sa)));
	}
	free_ifi_info_plus(ifihead);
	exit(0);
}

/* From Steven's unpv13e/lip/ */
char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen){
    static char str[128];		/* Unix domain is largest */

	switch (sa->sa_family) {
	case AF_INET: {
		struct sockaddr_in	*sin = (struct sockaddr_in *) sa;

		if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
			return(NULL);
		return(str);
	}

#ifdef	IPV6
	case AF_INET6: {
		struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *) sa;

		if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
			return(NULL);
		return(str);
	}
#endif
	default:
		snprintf(str, sizeof(str), "sock_ntop_host: unknown AF_xxx: %d, len %d",
				 sa->sa_family, salen);
		return(str);
	}
    return (NULL);
}
