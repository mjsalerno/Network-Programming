#ifndef TOUR_H
#define TOUR_H

#include <pthread.h>

#include "common.h"
#include "debug.h"


#define IPPROTO_TOUR 176
#define TOURHDR_MIN (sizeof(struct tourhdr) + 4)

/* Extract the tourhdr pointer from an ip header pointer */
#define EXTRACT_TOURHDRP(ip_pktp) ((struct tourhdr*)(IP4_HDRLEN + ((char*)(ip_pktp))))

/* The tour has ended. */
#define TOUR_IS_OVER(hdr) ((hdr)->index >= (hdr)->num_ips)

/* These return a pointer to a stuct in_addr: */
/* CURR should always be your IP */
#define TOUR_PREV(hdr) (((struct in_addr*)(hdr)) + (hdr)->index + 3 - 1)
#define TOUR_CURR(hdr) (((struct in_addr*)(hdr)) + (hdr)->index + 3)
#define TOUR_NEXT(hdr) (((struct in_addr*)(hdr)) + (hdr)->index + 3 + 1)

struct tourhdr {
    struct in_addr g_ip;
    in_port_t g_port;
    uint16_t index;    /* (struct in_addr)trhdr + trhdr->index + 3 */
    uint32_t num_ips;
    /* struct in_addr a_1 */
    /* struct in_addr a_2 */
    /* ... */
    /* struct in_addr a_num_ips */
};

int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr);

int validate_ip_tour(struct ip *ip_pktp, size_t n, struct sockaddr_in *srcaddr);
int validate_mcast();
int validate_icmp();

#endif
