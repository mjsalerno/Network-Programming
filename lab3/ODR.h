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
    time_t ttl;             /* time_to_live timestamp */
    int port;               /* 0 reserved for timeservers */
    char sun_path[108];
};


#define T_RREQ 0
#define T_RREP 1
#define T_DATA 2
#define ODR_MSG_MAX 1515
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
    uint8_t force_redisc:1;
};

/**
*  The pending queue of T_DATA messages waiting for routes.
*  NOTE: MISLEADING because the odr_msg is followed by data.
*`
*  |---------- msg_node --------------|
*  |-- next --|------- odr_msg -------|---- odr_msg.len bytes of data ----|
*      ||
*      V
*      |----msg_node----|
*/
struct msg_node {
    struct msg_node *next;
    struct odr_msg *msg;
};

struct msg_queue {
    struct msg_node *head;
    struct msg_node *tail;
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
#define NEW_ROUTE 0x1
#define EFF_ROUTE 0x2
int add_route(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN],
        unsigned char mac_next_hop[ETH_ALEN], int iface_index, uint16_t num_hops,
        uint32_t broadcast_id, int staleness, int* flags);
int find_route_index(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN]);
int delete_route_index(struct tbl_entry route_table[NUM_NODES], int index);

/* funcs for raw pkt stuffs */
size_t craft_frame(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len);
void broadcast(int rawsock, struct hwa_info *hwa_head, char *data, size_t data_len, int except);
void send_on_iface(int rawsock, char* data, size_t data_len,
        int dst_if, unsigned char dst_mac[ETH_ALEN]);

/* funcs for odr_msg{} */
void craft_rreq(struct odr_msg *m, char *srcip, char *dstip, int force_redisc, uint32_t broadcastID);
void craft_rrep(struct odr_msg *m, char *srcip, char *dstip, int force_redisc, int num_hops);

/* funcs for msg_queue{} */
void queue_store(struct msg_queue *queue, struct odr_msg *m);

#endif /* ODR_H */
