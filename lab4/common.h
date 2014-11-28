#ifndef COMMON_H
#define COMMON_H

#include <netinet/ip.h>
#include <string.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <errno.h>

#include "debug.h"

#define PROTO 0
#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8

size_t craft_eth(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
size_t craft_ip(int index, struct sockaddr_ll* raw_addr, void* buff, in_addr_t src_ip, in_addr_t dst_ip, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
size_t craft_icmp(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len);
uint16_t csum(void* data, size_t len);

#endif /*COMMON_H*/