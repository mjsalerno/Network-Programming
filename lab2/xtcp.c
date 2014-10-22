#include "xtcp.h"

void print_xtxphdr(struct xtcphdr *hdr) {
    printf("seq     : %u\n", hdr->seq);
    printf("flags   : %u\n", hdr->flags);
    printf("ack_seq : %u\n", hdr->ack_seq);
    printf("advwin  : %u\n", hdr->advwin);
}

void make_xtcphdr(void *hdr, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, void *data, size_t datalen){
    struct xtcphdr *realhdr = (struct xtcphdr*)hdr;
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
    memcpy(( (char*)(hdr) + DATAOFFSET), data, datalen);
}
