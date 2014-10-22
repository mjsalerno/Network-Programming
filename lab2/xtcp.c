#include "xtcp.h"

void print_xtxphdr(struct xtcphdr *hdr) {
    printf("seq     : %u\n", hdr->seq);
    printf("flags   : %u\n", hdr->flags);
    printf("ack_seq : %u\n", hdr->ack_seq);
    printf("advwin  : %u\n", hdr->advwin);
}

void make_xtcphdr(struct xtcphdr *hdr, uint32_t seq, uint32_t ack_seq,
        uint16_t flags, uint16_t advwin, char *data, size_t datalen){
    hdr->seq = seq;
    hdr->flags = flags;
    hdr->ack_seq = ack_seq;
    hdr->advwin = advwin;
    memcpy(( (char*)(hdr) + 12), data, datalen);
}
