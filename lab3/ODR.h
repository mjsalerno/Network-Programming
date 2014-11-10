#ifndef ODR_H
#define ODR_H

#include <linux/if_ether.h>

#include "common.h"
#include "get_hw_addrs.h"

#define MAX_NUM_SVCS 100
struct svc_entry {
    int port;                   /* 0 reserved for timeservers */
    char sun_path[108];
};

#define MESG_OFFSET (sizeof(struct Mesg))
/* fixme: rough estimate of the max message length */
#define MAX_MESG_SIZE (MESG_OFFSET + 1500)
struct Mesg {
    int port;
    int flag;
    size_t len;
    char ip[INET_ADDRSTRLEN];
    /* followed by len bytes of data */
};

/* funcs for manipulating hwa_info{} list */
void print_hw_addrs(struct hwa_info	*hwahead);
void rm_eth0_lo(struct hwa_info	**hwahead);

/* funcs for svc_entry{} array/list */
void svc_init(struct svc_entry *svcs, size_t len);
void svc_add(struct svc_entry *svcs, int *nxt_port, struct sockaddr_un *svc_addr);
void svc_rm(struct svc_entry* array, int *nxt_port, int rm_port);
int svc_contains_port(struct svc_entry *svcs, int port);
int svc_contains_path(struct svc_entry *svcs, struct sockaddr_un *svc_addr);

#endif /* ODR_H */
