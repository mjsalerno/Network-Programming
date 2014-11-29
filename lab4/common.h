#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <errno.h>

#include "debug.h"


#define EVER ;;
#define PROTO 0x2691
#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8
#define BUFSIZE 512
#define ARP_PATH "/tmp/cse533-11_arp"

struct hwaddr {
    int             sll_ifindex;	 /* Interface number */
    unsigned short  sll_hatype;	     /* Hardware type */
    unsigned char   sll_halen;		 /* Length of address */
    unsigned char   sll_addr[8];	 /* Physical layer address */
};


size_t craft_eth(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
size_t craft_ip(int index, struct sockaddr_ll* raw_addr, void* buff, in_addr_t src_ip, in_addr_t dst_ip, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
size_t craft_icmp(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
uint16_t csum(void* data, size_t len);

ssize_t write_n(int fd, char *buf, size_t n);

void print_hwa(char* mac, int len);

#endif /*COMMON_H*/