#include <stdint.h>
#include <stdio.h>

#ifndef XTCP_H
#define XTCP_H

#define FIN 0x1
#define SYN 0x2
#define RST 0x4
#define ACK 0x8

struct xtcphdr {

    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t win_size;
    char data[500];
};

void print_xtxphdr(struct xtcphdr *hdr);

#endif /*XTCP_H*/
