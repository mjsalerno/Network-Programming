#ifndef ARP_H
#define ARP_H

#include "common.h"
#include "get_hw_addrs.h"

struct arp_cache {
    struct hwaddr* hw;
    in_addr_t ip;
    struct arp_cache* next;
};

/*todo: make the actually arp thing*/
void add_arp(struct arp_cache** arp_head, in_addr_t ip, int sll_ifindex, unsigned short sll_hatype, unsigned char sll_halen, unsigned char   sll_addr[8]);
struct arp_cache* has_arp(struct arp_cache* arp_head, in_addr_t ip);
#endif