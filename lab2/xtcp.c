#include <inttypes.h>
#include "xtcp.h"
#include "debug.h"

void print_xtxphdr(struct xtcphdr *hdr) {
    int is_ack = 0;
    printf("|xtcp_hdr| seq:%u", hdr->seq);
    printf(", flags:");
    if((hdr->flags & FIN) == FIN){ printf("F"); }
    if((hdr->flags & SYN) == SYN){ printf("S"); }
    if((hdr->flags & RST) == RST){ printf("R"); }
    if((hdr->flags & ACK) == ACK){ printf("A"); is_ack = 1; }
    if(is_ack) {
        printf(", ack_seq:%u", hdr->ack_seq);
    }
    printf(", advwin:%u\n", hdr->advwin);
}

void make_pkt(void *hdr, uint16_t flags, uint16_t advwin, void *data, size_t datalen) {
    struct xtcphdr *realhdr = (struct xtcphdr*)hdr;
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
    memcpy(( (char*)(hdr) + DATA_OFFSET), data, datalen);
}

int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen) {

    ssize_t err;
    void *pkt = malloc(sizeof(struct xtcphdr) + datalen);
    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_xtxphdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

    /*todo: do WND/rtt stuff*/
    err = add_to_wnd(seq, pkt, (const char**)wnd);
    if(err < 0) {
        _DEBUG("%s\n", "The window was full, not sending");
        return -1;
    }

    err = send(sockfd, pkt, DATA_OFFSET + datalen, 0);
    if(err < 0) {
        perror("xtcp.srvsend()");
        free(pkt);
        return -2;
    }
    seq++;
    free(pkt);
    return 1;
}

int clisend(int sockfd, uint16_t flags, void *data, size_t datalen){
    ssize_t err;
    void *pkt = malloc(DATA_OFFSET + datalen);

    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_xtxphdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

    /*todo: do WND/rtt stuff*/

    /* simulate packet loss on sends */
    if(drand48() > pkt_loss_thresh) {
        err = send(sockfd, pkt, (DATA_OFFSET + datalen), 0);
        if (err < 0) {
            perror("xtcp.clisend()");
            free(pkt);
            return -1;
        }
    }
    else {
        /* fixme: remove prints? */
        printf("DROPPED PKT: ");
        ntohpkt((struct xtcphdr*)pkt);
        print_xtxphdr((struct xtcphdr*)pkt);
        htonpkt((struct xtcphdr*)pkt);
    }

    free(pkt);
    return 1;
}

int add_to_wnd(uint32_t index, const char* pkt, const char** wnd) {
    int n = (index + basewin) % advwin;
    if(n > advwin) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() result of mod (%d) was greater than window size (%" PRIu32 ")\n", n, index);
        return -1;
    }

    if(wnd[n] != NULL) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() index location already ocupied: %d\n", n);
        return -1;
    }

    wnd[n] = pkt;

    return 1;
}

const char* remove_from_wnd(uint32_t index, const char** wnd) {
    int n = (index + basewin) % advwin;
    const char* tmp;

    if(n > advwin) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() result of mod (%d) was greater than window size (%" PRIu32 ")\n", n, index);
        return NULL;
    }

    tmp = wnd[n];
    wnd[n] = NULL;

    return tmp;
}

void free_wnd(char** wnd) {
    int size = (sizeof(wnd)) / (sizeof(char *));
    char* tmp;
    int i;

    for(i = 0; i < size; ++i) {
        tmp = wnd[i];
        if(tmp != NULL) {
            free(tmp);
        }
    }

    free(wnd);
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
