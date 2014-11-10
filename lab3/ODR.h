#ifndef ODR_H
#define ODR_H

#include <stdio.h>
#include <stdlib.h>

#include "get_hw_addrs.h"

/* I don't think we should distinguish clients and servers here. right? */
struct entry {
    char local;                 /* 'y' if local, 'n' if on a remote machine */
    char ip[INET_ADDRSTRLEN];
    int port;                   /* 0 reserved for timeservers */
    char sun_path[108];
    time_t ts;
};

void print_hw_addrs(struct hwa_info	*hwahead);

#endif /* ODR_H */
