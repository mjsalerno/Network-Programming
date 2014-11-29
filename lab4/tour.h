#ifndef TOUR_H
#define TOUR_H

#include "common.h"
#include "debug.h"

#define IPPROTO_TOUR 176
/* Return a pointer to a stuct in_addr */
#define TOUR_PREV(hdr) (((struct in_addr*)(hdr)) + (hdr)->index - 1)
#define TOUR_CURR(hdr) (((struct in_addr*)(hdr)) + (hdr)->index)
#define TOUR_NEXT(hdr) (((struct in_addr*)(hdr)) + (hdr)->index + 1)

/* The tour has ended. */
#define TOUR_IS_OVER(hdr) (((hdr)->index - 3) == (hdr)->num_ips)

struct tourhdr {
    struct in_addr g_ip;
    uint16_t g_port;
    uint16_t index;    /* (struct in_addr)trhdr + trhdr->index */
    uint32_t num_ips;
    /* struct in_addr a_1 */
    /* struct in_addr a_2 */
    /* ... */
    /* struct in_addr a_num_ips */
};

int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);



#endif
