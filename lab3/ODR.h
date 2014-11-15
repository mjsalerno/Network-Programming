#ifndef ODR_H
#define ODR_H

#include <linux/if_ether.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <time.h>

#include "common.h"
#include "get_hw_addrs.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SVC_MAX_NUM 100
#define SVC_TTL 15
struct svc_entry {
    int port;                   /* 0 reserved for timeservers */
    char sun_path[108];
    time_t ttl;                 /* time_to_live timestamp */
};


#define T_RREQ 0
#define T_RREP 1
#define T_DATA 2
#define ODR_MSG_MAX 1522
/* type: 0 for RREQ, 1 for RREP, and 2 for app payload */
struct odr_msg {
    uint32_t broadcast_id;
    char type;
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t num_hops;
    uint16_t len;
    uint8_t do_not_rrep:1;
    uint8_t reroute:1;
};

struct tbl_entry {
    unsigned char mac_next_hop[ETH_ALEN];
    char ip_dst[INET_ADDRSTRLEN];
    int iface_index;
    uint16_t num_hops;
    uint32_t broadcast_id;
    time_t timestamp;
};

/* funcs for manipulating hwa_info{} list */
void print_hw_addrs(struct hwa_info	*hwahead);
void rm_eth0_lo(struct hwa_info	**hwahead);

/* funcs for svc_entry{} array/list */
void svc_init(struct svc_entry *svcs, size_t len);
int svc_update(struct svc_entry *svcs, struct sockaddr_un *svc_addr);

/* funcs for using the sockets */
int handle_unix_msg(int unixfd, struct svc_entry *svcs,
        struct odr_msg *m, size_t mlen, struct sockaddr_un *from_addr);
int deliver_app_mesg(int unixfd, struct odr_msg *m, size_t bytes);

/* route table stuff */
int add_route(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN], unsigned char mac_next_hop[ETH_ALEN], int iface_index, uint16_t num_hops, uint32_t broadcast_id, int staleness);
int find_route_index(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN]);

/* funcs for raw pkt stuffs */
void* craft_frame(int rawsock, int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len);

#endif /* ODR_H */
