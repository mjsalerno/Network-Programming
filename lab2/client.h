#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <math.h>
#include <pthread.h>

#include "debug.h"

extern double pkt_loss_thresh; /* packet loss percentage */
#define DROP_PKT() (drand48() < (pkt_loss_thresh))

/*
Performs handshakes.
Returns -1 on failure with perror() printed
        0 on success
*/
int handshakes(int serv_fd, struct sockaddr_in *serv_addr, char *fname);
int validate_hs2(struct xtcphdr* hdr, int len);
void *consumer_main(void *);
int consumer_read();
int init_wnd_mutex(void);

int clirecv(int sockfd, struct window* w);
void clisend(int sockfd, struct xtcphdr *pkt, size_t datalen);

#endif
