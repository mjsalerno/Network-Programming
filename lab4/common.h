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
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <netdb.h>
#include <signal.h>

#include "debug.h"


#define EVER ;;
#define ARP_ETH_PROTO 0x2691
/* should be 20 bytes */
#define IP4_HDRLEN (sizeof(struct ip))
/* should be 8 bytes */
#define ICMP_HDRLEN (sizeof(struct icmp))
#define BUFSIZE 512
#define ARP_PATH "/tmp/cse533-11_arp"
#define TOUR_GRP_IP "236.236.236.236"
#define TOUR_GRP_PORT 2691
#define TOUR_IP_ID 0x2691

#ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
#endif



void craft_eth(void* eth_buf, struct sockaddr_ll* raw_addr, unsigned char *src_mac, unsigned char *dst_mac, int ifindex);
void craft_ip(void* ip_pktbuf, uint8_t proto, u_short ip_id, struct in_addr src_ip, struct in_addr dst_ip, size_t paylen);
void print_ip(struct ip* ip_pkt);
unsigned char*extract_target_pa(struct arphdr *arp);
unsigned char* extract_sender_hwa(struct arphdr* arp);
unsigned char* extract_sender_pa(struct arphdr* arp);
size_t craft_arp(char* buf, uint16_t id, unsigned short int ar_op,  unsigned short int ar_pro, unsigned short int ar_hrd, unsigned char ar_sha[ETH_ALEN], unsigned char ar_sip[4], unsigned char ar_tha[ETH_ALEN], unsigned char ar_tip[4]);
uint16_t csum(void* data, size_t len);

/* Try to write n bytes of the buffer. Checks for EINTR */
ssize_t write_n(int fd, void *buf, size_t n);

/* Print generic hardware address of length len. */
void print_hwa(unsigned char* mac, char mac_len);

/* Convert an IP to host name */
char *getvmname(struct in_addr vmaddr);
int gethostname_ip(char *host_name, struct in_addr *host_ip);

#endif /*COMMON_H*/