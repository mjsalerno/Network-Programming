#ifndef ARP_H
#define ARP_H

#include "common.h"
#include "get_hw_addrs.h"

struct arp_cache {
    unsigned char      sll_addr[8];  /* Physical layer address */
    struct   sockaddr  *ip_addr;	 /* IP address */
    struct arp_cache* next;
};

/*todo: make the actually arp thing*/

#endif