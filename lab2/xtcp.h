#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>

#ifndef XTCP_H
#define XTCP_H

#define FIN 0x1
#define SYN 0x2
#define RST 0x4
#define ACK 0x8

#define DATA_OFFSET 12
#define MAX_PKT_SIZE 512
#define MAX_DATA_SIZE (MAX_PKT_SIZE - DATA_OFFSET)

#define TIME_OUT 2

/* for extern'ing in client and server */
uint32_t seq;           /* SEQ number */
uint32_t ack_seq;       /* ACK number */

uint16_t advwin;        /* current advwin */
int basewin;            /* index of the window base */
char** wnd;             /* the actual window */

double pkt_loss_thresh; /* packet loss percentage */

struct xtcphdr {
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t advwin;
};

void print_xtxphdr(struct xtcphdr *hdr);
/*
Example:
void *packet = malloc(sizeof(struct xtcphdr) + datalen)
make_pkt(packet, .....)
 */
void make_pkt(void *hdr, uint16_t flags, uint16_t advwin, void *data, size_t datalen);

int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen);
int clisend(int sockfd, uint16_t flags, void *data, size_t datalen);

int add_to_wnd(uint32_t index, const char* pkt, const char** wnd);
const char* remove_from_wnd(uint32_t index, const char** wnd);
void free_wnd(char** wnd);
char** init_wnd();

void ntohpkt(struct xtcphdr *hdr);
void htonpkt(struct xtcphdr *hdr);

#endif /*XTCP_H*/
