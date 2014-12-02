#include "ARP.h"

int             sll_ifindex;	 /* Interface number */
unsigned short  sll_hatype;	     /* Hardware type */
unsigned char   sll_halen;		 /* Length of address */
unsigned char   sll_addr[8];	 /* Physical layer address */

void add_arp(struct arp_cache** arp_head, in_addr_t ip, int sll_ifindex, unsigned short sll_hatype, unsigned char sll_halen, unsigned char   sll_addr[8]) {
    struct arp_cache* ptr = *arp_head;
    struct arp_cache* prev = ptr;

    for(; ptr != NULL; prev = ptr, ptr = ptr->next);

    prev->next = malloc(sizeof(struct arp_cache));
    ptr = prev->next;
    if(ptr == NULL) {
        _ERROR("%s\n", "malloc failed: add_arp");
        exit(EXIT_FAILURE);
    }

    ptr->next = NULL;
    memcpy(ptr->hw->sll_addr, sll_addr, 8);
    memcpy(&ptr->hw->sll_halen, &sll_halen, 1);
    ptr->hw->sll_hatype = sll_hatype;
    ptr->hw->sll_ifindex = sll_ifindex;
    memcpy(&ptr->ip, &ip, sizeof(in_addr_t));

}

struct arp_cache* has_arp(struct arp_cache* arp_head, in_addr_t ip) {
    struct arp_cache* ptr = arp_head;
    for(; ptr != NULL && ( ptr->ip == ip); ptr = ptr->next);
    return ptr;
}