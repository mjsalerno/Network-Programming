#ifndef ARP_H
#define ARP_H

#include "common.h"
#include "api.h"
#include "get_hw_addrs.h"
#include <inttypes.h>

struct arp_cache {
    struct hwaddr hw;
    in_addr_t ip;
    int fd;
    struct arp_cache* next;
};

struct arp_cache*  add_arp(struct arp_cache** arp_head, in_addr_t ip, int sll_ifindex, unsigned short sll_hatype, unsigned char sll_halen, unsigned char   sll_addr[8], struct hwa_ip* iface, int fd);
struct arp_cache*  add_part_arp(struct arp_cache** arp_head, in_addr_t ip, int fd);
struct arp_cache* get_arp(struct arp_cache* arp_head, in_addr_t ip);
struct hwa_ip* is_my_ip(struct hwa_ip* head, in_addr_t ip);
void print_arp(struct arphdr* arp);
void free_arp_cache(struct arp_cache** head);
#endif