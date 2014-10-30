#ifndef XTCP_H
#define XTCP_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "common.h"


#define FIN 0x1
#define SYN 0x2
#define RST 0x4
#define ACK 0x8

#define DATA_OFFSET 12
#define MAX_PKT_SIZE 512
#define MAX_DATA_SIZE (MAX_PKT_SIZE - DATA_OFFSET)

#define TIME_OUT 2

extern double pkt_loss_thresh; /* packet loss percentage */
#define DROP_PKT() (drand48() < (pkt_loss_thresh))

struct xtcphdr {
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags;
    uint16_t advwin;
    /*uint16_t datalen;*/
};

struct win_node {
    int datalen;
    struct xtcphdr* pkt;
    struct win_node* next;
};

struct window {
    int maxsize;
    int numpkts; /* for client this is the number of total ordered/unordered pkts */
    int lastadvwinrecvd;
    uint32_t servlastackrecv;
    uint32_t servlastseqsent;
    int cwin;
    int ssthresh;
    int dupacks;
    uint32_t clitopaccptpkt;
    uint32_t clilastunacked;
    struct win_node *base;
};

void ackrecvd(struct window*, struct xtcphdr*);



void print_hdr(struct xtcphdr *hdr);
void *alloc_pkt(uint32_t seqn, uint32_t ack_seqn,
        uint16_t flags, uint16_t adv_win, void *data, size_t datalen);

void ntohpkt(struct xtcphdr *hdr);
void htonpkt(struct xtcphdr *hdr);

int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen,
        char **wnd, int is_new, uint16_t* cli_wnd);
void srv_add_send(int sockfd, void* data, size_t datalen, uint16_t flags, struct window *w);
void new_ack_recvd(struct window *window, struct xtcphdr *pkt);


void clisend_lossy(int sockfd, void *pkt, size_t datalen);


void free_window(struct win_node* head);
struct window* init_window(int maxsize, uint32_t srv_last_seq_sent, uint32_t srv_last_ack_seq_recvd,
        uint32_t cli_top_accept_seqn, uint32_t cli_last_seqn_recvd);
void print_window(struct window *windo);
void srv_send_base(int sockfd, struct window *w);
int cli_add_send(int sockfd, struct xtcphdr *pkt, int datalen, struct window* w);

#endif /*XTCP_H*/
