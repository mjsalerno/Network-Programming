#include "ARP.h"

void add_arp(struct arp_cache** arp_head, unsigned char sll_addr[8], struct sockaddr *ip_addr, struct arp_cache* next) {
    struct arp_cache* ptr = *arp_head;
    struct arp_cache* prev;

    for(; ptr != NULL; prev = ptr, ptr = ptr->next);

    prev->next = malloc(sizeof(struct arp_cache));
    ptr = prev->next;
    if(ptr == NULL) {
        _ERROR("%s\n", "malloc failed: add_arp");
        exit(EXIT_FAILURE);
    }

    ptr->next = NULL;
    memcpy(ptr->sll_addr, sll_addr, 8);
    memcpy(ptr->ip_addr, ip_addr, sizeof(struct sockaddr));

}