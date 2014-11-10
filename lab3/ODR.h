#ifndef ODR_H
#define ODR_H

#include <stdio.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include "common.h"
#include "get_hw_addrs.h"

#define MAX_NUM_SVCS 100

struct svc_entry {
    int port;                   /* 0 reserved for timeservers */
    char sun_path[108];
};

#define MESG_OFFSET sizeof(struct Mesg)
struct Mesg {
    int port;
    int flag;
    size_t len;
    char ip[INET_ADDRSTRLEN];
    /* followed by len bytes of data */
};

struct tbl_entry {
    char mac_next_hop[ETH_ALEN];
    int iface_index;
    int num_hops;
    time_t timestamp;
    struct tbl_entry* next;
};

void print_hw_addrs(struct hwa_info	*hwahead);
void* craft_frame(int rawsock, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len);

#endif /* ODR_H */
