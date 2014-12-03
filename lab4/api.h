#ifndef ARP_API_H
#define ARP_API_H

#include "common.h"

struct hwaddr {
    int             sll_ifindex;	 /* Interface number */
    unsigned short  sll_hatype;	     /* Hardware type */
    unsigned char   sll_halen;		 /* Length of address */
    unsigned char   sll_addr[8];	 /* Physical layer address */
};

int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);

#endif /* ARP_API_H */
