#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef XTCP_H
#define XTCP_H

#define FIN 0x1
#define SYN 0x2
#define RST 0x4
#define ACK 0x8

#define DATAOFFSET 12

struct xtcphdr {

    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t advwin;
};

void print_xtxphdr(struct xtcphdr *hdr);
/*
Example:
void *segment = malloc(sizeof(struct xtcphdr) + datalen)
make_xtcphdr(segment, .....)
 */
void make_xtcphdr(void *hdr, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, void *data, size_t datalen);

#endif /*XTCP_H*/
