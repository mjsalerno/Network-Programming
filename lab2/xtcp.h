#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#ifndef XTCP_H
#define XTCP_H

#define FIN 0x1
#define SYN 0x2
#define RST 0x4
#define ACK 0x8

#define DATA_OFFSET 12
#define MAX_PKT_SIZE 512
#define MAX_DATA_SIZE (MAX_PKT_SIZE - DATA_OFFSET)

struct xtcphdr {

    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t advwin;

};

struct window {

    struct xtcphdr*  hdr;
    void*            data;
    int              datasize;
    struct window*   next;

};

void print_xtxphdr(struct xtcphdr *hdr);
/*
Example:
void *packet = malloc(sizeof(struct xtcphdr) + datalen)
make_pkt(packet, .....)
 */
void make_pkt(void *hdr, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, void *data, size_t datalen);

int srvsend(int sockfd, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, void *data, size_t datalen);

void ntohpkt(struct xtcphdr *hdr);
void htonpkt(struct xtcphdr *hdr);

#endif /*XTCP_H*/
