#include "xtcp.h"

void print_xtxphdr(struct xtcphdr *hdr) {
    printf("seq     : %u\n", hdr->seq);
    printf("flags   : %u\n", hdr->flags);
    printf("ack_seq : %u\n", hdr->ack_seq);
    printf("advwin  : %u\n", hdr->advwin);
}

void make_pkt(void *hdr, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, void *data, size_t datalen) {
    struct xtcphdr *realhdr = (struct xtcphdr*)hdr;
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
    memcpy(( (char*)(hdr) + DATAOFFSET), data, datalen);
}

void ntohpkt(struct xtcphdr *hdr) {
    hdr->seq = ntohl(hdr->seq);
    hdr->ack_seq = ntohl(hdr->ack_seq);
    hdr->flags = ntohs(hdr->flags);
    hdr->advwin = ntohs(hdr->advwin);
}

void htonpkt(struct xtcphdr *hdr) {
    hdr->seq = htonl(hdr->seq);
    hdr->ack_seq = htonl(hdr->ack_seq);
    hdr->flags = htons(hdr->flags);
    hdr->advwin = htons(hdr->advwin);
}
