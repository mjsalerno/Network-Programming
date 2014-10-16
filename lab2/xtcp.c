#include "xtcp.h"

void print_xtxphdr(struct xtcphdr *hdr) {
    printf("seq     : %u\n", hdr->seq);
    printf("flags   : %u\n", hdr->seq);
    printf("ack_seq : %u\n", hdr->seq);
    printf("win_size: %u\n", hdr->seq);
}
