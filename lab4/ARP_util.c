#include "ARP.h"

void add_arp(struct arp_cache** arp_head, in_addr_t ip, int sll_ifindex, unsigned short sll_hatype, unsigned char sll_halen, unsigned char   sll_addr[8], struct hwa_ip* iface, int fd) {
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
    if(sll_addr != NULL)
        memcpy(ptr->hw.dst_addr, sll_addr, 8);
    memcpy(&ptr->ip, &ip, sizeof(in_addr_t));
    ptr->hw.dst_halen = sll_halen;
    ptr->hw.dst_hatype = sll_hatype;
    ptr->hw.src_ifindex = sll_ifindex;
    ptr->fd = fd;

    if(iface != NULL) {
        memcpy(ptr->hw.src_addr, iface->if_haddr, ETH_ALEN);
    }

    ptr->hw.src_halen = 6;
    ptr->hw.src_hatype = sll_hatype;

    _DEBUG("%s", "Added this to arp_cache\n");
    print_hwaddr(&ptr->hw);\

}

void add_part_arp(struct arp_cache** arp_head, in_addr_t ip, int fd) {
    struct in_addr ip_struc;

    ip_struc.s_addr = ip;
    _DEBUG("adding for ip: %s\n", inet_ntoa(ip_struc));

    add_arp(arp_head, ip, -1, 0, 6, NULL, NULL, fd);
}

struct arp_cache* get_arp(struct arp_cache* arp_head, in_addr_t ip) {
    struct arp_cache* ptr = arp_head;
    struct in_addr ip_struc;

    ip_struc.s_addr = ip;
    _DEBUG("looking for ip: %s\n", inet_ntoa(ip_struc));

    for(; ptr != NULL; ptr = ptr->next) {
        if(ptr->ip == ip) {
            _DEBUG("%s\n", "FOUND MATCH!!");
            break;
        }
        ip_struc.s_addr = ptr->ip;
        _DEBUG("not a match ip: %s\n", inet_ntoa(ip_struc));
    }

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

void free_arp_cache(struct arp_cache* node) {

    struct arp_cache* prev = node;

    if(node != NULL)
        node = node->next;
    else
        return;

    if(node == NULL) {
        free(prev);
        return;
    }

    for(;node != NULL; prev = node, node = node->next) {
        free(prev);
    }

}

