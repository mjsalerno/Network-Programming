#ifndef ODR_H
#define ODR_H

#include <linux/if_ether.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include "common.h"
#include "get_hw_addrs.h"

#define SVC_MAX_NUM 100
#define SVC_TTL 15
struct svc_entry {
    int port;                   /* 0 reserved for timeservers */
    char sun_path[108];
    time_t ttl;                 /* time_to_live timestamp */
};

#define API_MSG_OFFSET (sizeof(struct api_msg))
/* fixme: rough estimate of the max message length */
#define API_MSG_MAX (API_MSG_OFFSET + 1500)
struct api_msg {
    int dst_port;
    char dst_ip[INET_ADDRSTRLEN];
    int reroute;
};

#define ODR_MSG_MAX 1522
/* type: 0 for RREQ, 1 for RREP, and 2 for app payload */
struct odr_msg {
    int type;
    int src_port;
    int dst_port;
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    int reroute;
    int do_not_rrep;
    int len;
};

struct tbl_entry {
    char mac_next_hop[ETH_ALEN];
    int iface_index;
    int num_hops;
    time_t timestamp;
    struct tbl_entry* next;
};

/* funcs for manipulating hwa_info{} list */
void print_hw_addrs(struct hwa_info	*hwahead);
void rm_eth0_lo(struct hwa_info	**hwahead);

/* funcs for svc_entry{} array/list */
void svc_init(struct svc_entry *svcs, size_t len);
int svc_add(struct svc_entry *svcs, struct sockaddr_un *svc_addr);
int svc_contains_port(struct svc_entry *svcs, int port);
int svc_contains_path(struct svc_entry *svcs, struct sockaddr_un *svc_addr);

/* funcs for using the sockets */
int recvd_app_mesg(size_t bytes, struct api_msg *m, struct sockaddr_un *from_addr);
int deliver_app_mesg(int unixfd, struct api_msg *m, size_t bytes);

/* funcs for raw pkt stuffs */
void* craft_frame(int rawsock, int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len);

#endif /* ODR_H */
