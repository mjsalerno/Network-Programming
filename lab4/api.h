#ifndef ARP_API_H
#define ARP_API_H

#include "common.h"

struct hwaddr {
    int src_ifindex;	 /* Interface number */
    unsigned short dst_hatype;	 /* Hardware type */
    unsigned char dst_halen;	 /* Length of address */
    unsigned char  dst_addr[8];	 /* Physical layer address */
    unsigned short src_hatype;	 /* Hardware type */
    unsigned char src_halen;	 /* Length of address */
    unsigned char  src_addr[8];	 /* Physical layer address */
};

int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);
void print_hwaddr(struct hwaddr* addr);

#endif /* ARP_API_H */
