#include "xtcp.h"

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

int srvsend(int sockfd, uint16_t flags, uint16_t advwin, void *data, size_t datalen) {

    ssize_t err;
    void* pkt = malloc(MAX_PKT_SIZE);
    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_xtxphdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);
    
    /*todo: do WND/rtt stuff*/

    err = send(sockfd, pkt, DATA_OFFSET + datalen, 0);
    if(err < 0) {
        perror("xtcp.srvsend()");
        free(pkt);
        return -1;
    }

    free(pkt);
    return 1;
}

/*private function dont make proto*/
void fwh(struct window* at, struct window* head) {
    if(at->next != head)
        fwh(head->next, head);

    free(at->pkt);
    free(at);

}

void free_windows(struct window* head) {
    fwh(head, head);
}

struct window* malloc_windows(int count) {
    int i;
    struct window* head = NULL;
    struct window* ptr = NULL;

    if(count < 1) {
        return NULL;
    }

    head = malloc(sizeof(struct window));
    ptr = head->next;

    for(i = 1; i < count; ++i) {
        ptr = malloc(sizeof(struct window));
        ptr = ptr->next;
    }

    ptr = head;
    return head;
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
