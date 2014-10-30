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

#define E_ISFULL   -4
/*#define E_WASREMOVED    -3*/
/*#define E_CANTFIT       -2*/
#define E_OCCUPIED -1

/* for extern'ing in client and server */
uint32_t seq;           /* SEQ number */
uint32_t ack_seq;       /* ACK number */

uint16_t advwin;        /* current advwin */

extern double pkt_loss_thresh; /* packet loss percentage */

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
    int lastadvwinrecvd;
    uint32_t servlastackrecv;
    uint32_t servlastpktsent;
    int cwin;
    int ssthresh;
    int dupacks;
    uint32_t clitopaccptpkt;
    uint32_t clilastpktrecv;
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


/**
* clirecv -- for the client/receiver/acker
* Returns: >0 bytes recv'd
*           0 on FIN recv'd
*          -1 on RST recv'd
*          -2 on failure, with perror printed
* DESC:
* Blocks until a packet is recv'ed from sockfd. If it's a FIN, immediately return 0.
* If it's not a FIN then try to drop it based on pkt_loss_thresh.
* -if dropped, pretend it never happened and continue to block in select()
* -if kept:
*   -if RST then return -1
*   -else ACK
* NOTES:
* Sends ACKs and duplicate ACKS.
* If a dup_ack is sent go back to block in select().
* NULL terminates the data sent.
*/
int clirecv(int sockfd, struct window* w)

void clisend_lossy(int sockfd, void *pkt, size_t datalen);
int cli_ack(int sockfd, char **wnd);
int cli_dup_ack(int sockfd);
void clisend(int sockfd, uint16_t flags, void *data, size_t datalen);

void free_window(struct win_node* head);
struct window* init_window(int maxsize, uint32_t srv_last_seq_sent, uint32_t srv_last_ack_seq_recvd,
        uint32_t cli_top_accept_seqn, uint32_t cli_last_seqn_recvd);
void print_window(struct window *windo);

#endif /*XTCP_H*/
