#ifndef PING_H
#define PING_H 1

#include <pthread.h>
#include "common.h"
#include "api.h"
#include "debug.h"

#define PING_ICMP_ID 0x2691

#define EXTRACT_ICMPHDRP(ip_pktp) ((struct icmp*)(IP4_HDRLEN + ((char*)(ip_pktp))))

void *ping_sender(void *pgsender_and_addr);
void *ping_recver(void *pgrecverp);
int filter_ip_icmp(struct ip *ip_pktp, size_t n);
void craft_echo_reply(void *icmp_buf, uint16_t seq, void *data, size_t data_len);

#endif
