#include "ARP.h"
#include <inttypes.h>

int             sll_ifindex;	 /* Interface number */
unsigned short  sll_hatype;	     /* Hardware type */
unsigned char   sll_halen;		 /* Length of address */
unsigned char   sll_addr[8];	 /* Physical layer address */

void add_arp(struct arp_cache** arp_head, in_addr_t ip, int sll_ifindex, unsigned short sll_hatype, unsigned char sll_halen, unsigned char   sll_addr[8]) {
    struct arp_cache* ptr = *arp_head;
    struct arp_cache* prev = ptr;

    for(; ptr != NULL; prev = ptr, ptr = ptr->next);

    if(prev == NULL) {
        ptr = malloc(sizeof(struct arp_cache));
        *arp_head = ptr;
    } else {
        prev->next = malloc(sizeof(struct arp_cache));
        ptr = prev->next;
    }
    if(ptr == NULL) {
        _ERROR("%s\n", "malloc failed: add_arp");
        exit(EXIT_FAILURE);
    }

    memset(ptr, 0, sizeof(struct arp_cache));

    ptr->next = NULL;
    memcpy(ptr->hw.sll_addr, sll_addr, 8);
    memcpy(&ptr->hw.sll_halen, &sll_halen, 1);
    ptr->hw.sll_hatype = sll_hatype;
    ptr->hw.sll_ifindex = sll_ifindex;
    memcpy(&ptr->ip, &ip, sizeof(in_addr_t));

}

struct arp_cache* has_arp(struct arp_cache* arp_head, in_addr_t ip) {
    struct arp_cache* ptr = arp_head;
    for(; ptr != NULL && ( ptr->ip == ip); ptr = ptr->next);
    return ptr;
}

struct hwa_ip* is_my_ip(struct hwa_ip* head, in_addr_t ip) {
    struct hwa_ip* ptr = head;
    struct in_addr ip_struc;

    ip_struc.s_addr = ip;
    _DEBUG("looking for: %s\n", inet_ntoa(ip_struc));

    for(; ptr != NULL; ptr = ptr->next) {
        if(head->ip_addr.sin_addr.s_addr == ip) {

            return ptr;
        } else {
            _DEBUG("not a match: %s\n", inet_ntoa(head->ip_addr.sin_addr));
        }
    }

    return ptr;
}

void print_arp(struct arphdr* arp) {
    unsigned char* ptr = (unsigned char*)(arp+1);
    struct in_addr ip;

    /*unsigned char __ar_sha[ETH_ALEN];	 Sender hardware address.
    unsigned char __ar_sip[4];		 Sender IP address.
    unsigned char __ar_tha[ETH_ALEN];	 Target hardware address.
    unsigned char __ar_tip[4];		 Target IP address.*/

    printf("============ARP============\n");

    printf("sender hwa: ");
    print_hwa(ptr, ETH_ALEN);
    printf("\n");

    ptr += arp->ar_hln;
    ip.s_addr = *(uint32_t*)(ptr);
    printf("sender ip: %s\n", inet_ntoa(ip));

    ptr += arp->ar_pln;
    printf("target hwa: ");
    print_hwa(ptr, ETH_ALEN);
    printf("\n");

    ptr += arp->ar_hln;
    ip.s_addr = *(uint32_t*)(ptr);
    printf("target ip: %s\n", inet_ntoa(ip));

    printf("===========================\n");

}