#include <inttypes.h>
#include "xtcp.h"
#include "debug.h"

static int max_wnd_size;/* initialized by init_wnd() */

void print_hdr(struct xtcphdr *hdr) {
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

void print_wnd(const char** wnd) {
    printf("print_wnd is not done\n");
    /*int i;

    for(i = 0; i < max_wnd_size; ++i) {

    }*/
}

int has_packet(uint32_t index, const char** wnd) {
    int n = get_wnd_index(index);
    return wnd[n] != NULL;
}

uint32_t get_wnd_index(uint32_t n) {
    return (n + basewin) % max_wnd_size;
}

void make_pkt(void *hdr, uint16_t flags, uint16_t advwin, void *data, size_t datalen) {
    struct xtcphdr *realhdr = (struct xtcphdr*)hdr;
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
    memcpy(( (char*)(hdr) + DATA_OFFSET), data, datalen);
}

int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen, char** wnd) {

    ssize_t err;
    void *pkt = malloc(sizeof(struct xtcphdr) + datalen);
    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_hdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

    /*todo: do WND/rtt stuff*/
    err = add_to_wnd(seq, pkt, (const char**)wnd);
    if(err == -1) {          /* out of bounds */
        _DEBUG("ERROR: %s\n", "out of bounds");
        free(pkt);
        return -3;

    } else if(err == -2) {  /* full window */
        _DEBUG("%s\n", "The window was full, not sending");
        free(pkt);
        return -1;

    } else if (err == 1) {
        _DEBUG("%s\n", "packet was added ...");
    } else {
        _DEBUG("ERROR: %s\n", "SOMETHING IS WRONG");
    }

    err = send(sockfd, pkt, DATA_OFFSET + datalen, 0);
    if(err < 0) {
        perror("xtcp.srvsend()");
        remove_from_wnd(seq, (const char **)wnd);
        free(pkt);
        return -2;
    }

    seq++;
    return 1;
}

char** init_wnd() {
    char** rtn;
    int i;
    max_wnd_size = advwin;

    rtn = malloc((size_t)(max_wnd_size * sizeof(char*)));

    for(i = 0; i < advwin; ++i)
        *(rtn+i) = 0;

    return rtn;
}

/**
* Adds packets that are already malloced to the window
* returns -1 if out of bounds
* returns -2 if window is full
* returns 1 on sucsess
*/
int add_to_wnd(uint32_t index, const char* pkt, const char** wnd) {
    int n = get_wnd_index(index);
    if(n > advwin) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() result of mod (%d) was greater than window size (%" PRIu32 ")\n", n, index);
        return -1;
    }

    if(wnd[n] != NULL) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() index location already ocupied: %d\n", n);
        return -2;
    }

    wnd[n] = pkt;

    return 1;
}

char* remove_from_wnd(uint32_t index, const char** wnd) {
    int n = get_wnd_index(index);
    const char* tmp;

    if(n > advwin) {
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() result of mod (%d) was greater than window size (%" PRIu32 ")\n", n, index);
        return NULL;
    }

    tmp = wnd[n];
    wnd[n] = NULL;

    return (char*)tmp;
}

void free_wnd(char** wnd) {
    int size = max_wnd_size;
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

int clisend(int sockfd, uint16_t flags, void *data, size_t datalen){
    ssize_t err;
    void *pkt = malloc(DATA_OFFSET + datalen);

    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_hdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

    /* todo: do WND/rtt stuff or in cli_ack, cli_dup_ack */

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
        print_hdr((struct xtcphdr *) pkt);
        htonpkt((struct xtcphdr*)pkt);
    }

    free(pkt);
    return 1;
}

int cli_ack(int sockfd) {
    int err;
    /*todo: window stuff*/
    err = clisend(sockfd, ACK, NULL, 0);
    return (int)err;
}

int cli_dup_ack(int sockfd) {
    int err;
    /*todo: window stuff*/
    err = clisend(sockfd, ACK, NULL, 0);
    return (int)err;
}
