#include <errno.h>
#include <cursesw.h>
#include "xtcp.h"


extern pthread_mutex_t w_mutex;

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

void print_hdr(struct xtcphdr *hdr) {
    int is_ack = 0;
    int any_flags = 0;
    printf("|hdr| <<<{ seq:%u", hdr->seq);
    printf(", flags:");
    if((hdr->flags & FIN) == FIN){ printf("F"); any_flags = 1; }
    if((hdr->flags & SYN) == SYN){ printf("S"); any_flags = 1; }
    if((hdr->flags & RST) == RST){ printf("R"); any_flags = 1; }
    if((hdr->flags & ACK) == ACK){ printf("A"); any_flags = 1; is_ack = 1; }
    if(!any_flags) {
        printf("0");
    }
    if(is_ack) {
        printf(", ack_seq:%u", hdr->ack_seq);
    }/*
    else{
        printf(", dlen:%u", hdr->datalen);
    }*/
    printf(", advwin:%u }>>>\n", hdr->advwin);
}

/**
* malloc's a pkt for you
*/
void *alloc_pkt(uint32_t seqn, uint32_t ack_seqn, uint16_t flags, uint16_t adv_win, void *data, size_t datalen) {
    struct xtcphdr *pkt;
    if(data == NULL || datalen <= 0){
        datalen = 0;
    }
    pkt= malloc(DATA_OFFSET + datalen);
    if(pkt == NULL){
        perror("ERROR: alloc_pkt().malloc()");
        exit(EXIT_FAILURE);
    }
    pkt->seq = seqn;
    pkt->flags = flags;
    pkt->ack_seq = ack_seqn;
    pkt->advwin = adv_win;
    memcpy(( (char*)(pkt) + DATA_OFFSET), data, datalen);
    return pkt;
}

/**
* Nice fancy print function. Extra goodies if DEBUG is defined.
*/
void print_window(struct window *w){
    struct win_node* head = w->base;
    struct win_node* curr = head;
    int n = 0;
    printf("|window| maxwnd: %6d, cwin: %6d, ssthresh: %6d, \n",
            w->maxsize, w->cwin, w->ssthresh);

    do {
        if(curr->datalen < 0){
            printf("_ ");
        }else {
            printf("X ");
        }
        curr = curr->next;
    } while(curr != head);

    #ifdef DEBUG
    printf("DEBUG |window| nodes:\n");
    do {
        printf("====== node %3d ========\n", n);
        printf("|Datalen: %15d |\n", curr->datalen);
        printf("|Pkt ptr: %15p |\n", (void *)curr->pkt);
        if(curr->pkt != NULL) {
            printf("|Pkt seq: %15"PRIu32" |\n", curr->pkt->seq);
        }else{
            printf("|Pkt seq: non pkt |\n");
        }
        printf("|Next:    %15p |\n", (void *)curr->next);
        printf("|This:    %15p |\n", (void *)curr);
        printf("====== node %3d =======\n", n);
        curr = curr->next;
    } while(curr != head);
    #endif
}

/* Only used by init_window */
struct win_node* alloc_window_nodes(size_t n) {
    struct win_node* head;
    struct win_node* tail;

    if(n < 1) {
        _DEBUG("passed invalid size: %d\n", (int)n);
        return NULL;
    }

    head = malloc(sizeof(struct win_node));
    if(head == NULL) {
        perror("alloc_window(): malloc failed");
        exit(EXIT_FAILURE);
    }
    head->datalen = -1;
    head->pkt = NULL;
    tail = head;

    for(n -= 1; n > 0; --n) {
        tail->next = malloc(sizeof(struct win_node));
        if(tail->next == NULL) {
            perror("alloc_window(): malloc failed");
            exit(EXIT_FAILURE);
        }
        tail = tail->next;
        tail->datalen = -1;
        tail->pkt = NULL;
    }

    tail->next = head;
    return head;
}

void free_window(struct win_node* head) {
    struct win_node* tail = head->next;
    struct win_node* tmp;

    while(tail != head) {
        tmp = tail;
        tail = tail->next;
        free(tmp);
    }

    free(head);
}

/**
* Called to init the sliding window.
*/
struct window* init_window(int maxsize, uint32_t srv_last_seq_sent, uint32_t srv_last_ack_seq_recvd,
        uint32_t cli_last_seqn_recvd, uint32_t cli_top_accept_seqn){

    struct window* w;
    w = malloc(sizeof(struct window));
    if(w == NULL){
        perror("ERROR init_window().malloc()");
        exit(EXIT_FAILURE);
    }
    w->maxsize = maxsize;
    w->lastadvwinrecvd = maxsize;
    w->servlastackrecv = srv_last_ack_seq_recvd;
    w->servlastseqsent = srv_last_seq_sent;
    /* todo: change to 1 */
    w->cwin = 65536;
    w->ssthresh = 65536;
    w->dupacks = 0;
    w->clitopaccptpkt = cli_top_accept_seqn;
    w->clilastpktrecv = cli_last_seqn_recvd;
    w->base = alloc_window_nodes((size_t)maxsize);
    if(w->base == NULL){
        perror("init_window().alloc_window()");
        exit(EXIT_FAILURE);
    }
    return w;
}

/**
* todo: Please mask sigalrm, sigprocmask()
* Used when a timeout occurs. Sends the pkt at the base.
*
* Caller MUST check if the packet CAN be sent. min(cwin, lastadvwinrecvd, maxsize)
*/
void srv_send_base(int sockfd, struct window *w) {
    ssize_t err;
    if(w == NULL) {
        fprintf(stderr, "ERROR: srv_send_base() w is NULL\n");
        exit(EXIT_FAILURE);
    }
    if(w->base == NULL) {
        fprintf(stderr, "ERROR: srv_send_base() w->base is NULL, init your window!\n");
        print_window(w);
        exit(EXIT_FAILURE);
    }
    if(w->base->pkt == NULL) {
        fprintf(stderr, "ERROR: srv_send_base() w->base->pkt is NULL, can't send NULL dummy!\n");
        print_window(w);
        exit(EXIT_FAILURE);
    }
    err = send(sockfd, w->base->pkt , (size_t)w->base->datalen, 0);
    if(err < 0){
        perror("ERROR srv_send_base().send()");
        exit(EXIT_FAILURE);
    }
}

/**
* todo: Please mask sigalrm, sigprocmask()
* Give me pkt in host order!! The window contains host order packets.
*
* Prints the window and the header sent.
*
*/
void srv_add_send(int sockfd, struct xtcphdr *pkt, int datalen, struct window *w){
    struct win_node *base;
    struct win_node *curr;
    int n = 0;
    int effectivesize;
    uint32_t seqtoadd;

    effectivesize = MIN(w->cwin, MIN(w->lastadvwinrecvd, w->maxsize));

    if(w == NULL) {
        fprintf(stderr, "ERROR: srv_add_send() w is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if(pkt == NULL) {
        fprintf(stderr, "ERROR: srv_add_send() pkt is NULL, can't add/send NULL!\n");
        exit(EXIT_FAILURE);
    }
    base = w->base;
    if(base == NULL) {
        fprintf(stderr, "ERROR: srv_add_send() w->base is NULL!\n");
        exit(EXIT_FAILURE);
    }

    if(effectivesize <= 0) {
        fprintf(stderr, "ERROR: srv_add_send() effective winsize: %d is <= 0\n", effectivesize);
        print_window(w);
        exit(EXIT_FAILURE);
    }

    seqtoadd = pkt->seq;
    if(seqtoadd != (w->servlastseqsent + 1)){
        fprintf(stderr, "ERROR: srv_add_send() seqtoadd: %"PRIu32" != "
                        "(w->servlastseqsent + 1): %"PRIu32"\n", seqtoadd,
                (w->servlastseqsent + 1));
        print_window(w);
        exit(EXIT_FAILURE);
    }
    curr = base;
    while(n < effectivesize) {
        /* inc n and move curr to the next*/
        n++;
        curr = curr->next;
        if(curr == NULL) {
            fprintf(stderr, "ERROR: srv_add_send() a win_node is NULL, init your window!\n");
            print_window(w);
            exit(EXIT_FAILURE);
        }
        if(curr == base) {
            fprintf(stderr, "ERROR: srv_add_send() reached the base while trying to add to window!\n");
            print_window(w);
            exit(EXIT_FAILURE);
        }
    }
    /* curr is now at the win_node we want? */
    if(curr->datalen >= 0 || curr->pkt != NULL) {
        fprintf(stderr, "ERROR: srv_add_send() curr->pkt not empty!\n");
        print_window(w);
        exit(EXIT_FAILURE);
    } else {
        _DEBUG("%s", "Found win_node for pkt. Adding to window.\n");
        curr->datalen = datalen;
        curr->pkt = pkt;
        w->servlastseqsent = seqtoadd;
    }

    htonpkt(pkt);
    n = (int) send(sockfd, pkt, (size_t)datalen, 0);
    if(n < 0) {
        _ERROR("%s\n", "srv_add_send().send()");
        exit(EXIT_FAILURE);
    }
    ntohpkt(pkt);
    printf("SENT ");
    print_hdr(pkt);
    print_window(w);
}



/**
* todo: Please mask sigalrm, sigprocmask()
*
*/
void new_ack_recvd(struct window *window, struct xtcphdr *pkt) {
    int count = 0;
    static int total_acks = 0;
    if(pkt->ack_seq > window->servlastseqsent) {
        _ERROR("Client ACKing something i never sent, last SEQ sent: %" PRIu32 " ACK got: %" PRIu32 "\n", window->servlastseqsent, pkt->ack_seq);
        exit(EXIT_FAILURE);
    } else if(pkt->ack_seq < window->servlastackrecv) {
        _NOTE("Client ACKing something already ACKed, base SEQ: %" PRIu32 " ACK got: %" PRIu32 "\n", window->base->pkt->seq, pkt->ack_seq);
        return;
    }

    if(window->cwin < window->ssthresh) {
        /* slow start */
        _DEBUG("we are in slow start cwin: %d, ssthresh: %d");
        window->cwin = window->cwin + 1;
        count = remove_aked_pkts(window, pkt);
        _DEBUG("number of ACKs: %d\n", count);
        window->cwin += count;
        _DEBUG("new cwin: %d\n", cwin);
    } else {
        /** todo: congestion cntrl
        *  count numacks
        *  if numacks == cwin
        *  then cwin++, num acks = 0
        **/
        count = remove_aked_pkts(window, pkt);
        total_acks += count;

        _DEBUG("number of ACKs: %d\n", count);
        _DEBUG("new total_acks: %d\n", total_acks);

        if(total_acks >= window->cwin) {
            window->cwin += 1;
            _DEBUG("incremented cwin, new cwin: %d\n", window->cwin);
        }
    }
}

int remove_aked_pkts(struct window *window, struct xtcphdr *pkt) {
    int rtn = 0;

    /* dup ack */
    if(window->servlastackrecv == pkt->ack_seq){
        _NOTE("%s\n", "got dup ack: %" PRIu32 "\n", pkt->ack_seq);
        window->dupacks++;
        _NOTE("%s\n", "new dupack: %" PRIu32 "\n", window->dupacks);
        /* todo: do fast retrans */
        return 0;
    }

    while(pkt->ack_seq > window->base->pkt->ack_seq) {
        rtn++;
        if(window->base == NULL || window->base->pkt == NULL) {
            _ERROR("%s\n", "was about to touch NULL");
            exit(EXIT_FAILURE);
        }

        _DEBUG("looking at %" PRIu32 ", have %" PRIu32 "\n", window->servlastackrecv, pkt->ack_seq);
        _DEBUG("%s\n", "freeing ACKed packet");
        free(window->base->pkt);
        window->base->datalen = -1;
    }

    window->lastadvwinrecvd = pkt->advwin;
    _DEBUG("New lastadvwinrecvd: %d\n", window->lastadvwinrecvd);

    return rtn;
}


int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen, char **wnd, int is_new, uint16_t* cli_wnd) {

    int err;
    void *pkt;

    if(*cli_wnd < 1) {
        if(!is_wnd_empty()) {
            _DEBUG("%s\n", "The client window was too small, pretending like my window is full ...");
            return -1;
        } else {
            _DEBUG("%s\n", "The client window was too small, but there isnothing to be ACKed... WHAT DO?");
        }
    }

    pkt = malloc(sizeof(struct xtcphdr) + datalen);
    make_pkt(pkt, flags, advwin, data, datalen);

    printf("SENDING: ");
    print_hdr((struct xtcphdr*)pkt);
    htonpkt((struct xtcphdr*)pkt);
    /* fixme: double check */
    /* todo: do WND/rtt stuff */
    if(is_new) {
        if (can_add_to_wnd(seq)) {
            err = add_to_wnd(seq, pkt, (const char **) wnd);
            if (err == E_OCCUPIED) {
                _DEBUG("ERROR: tried to re-insert seq: %"PRIu32"\n", seq);
                free(pkt);
                return -2;
            } else if (err == E_ISFULL) {
                _DEBUG("%s\n", "The window was full, not sending");
                free(pkt);
                return -1;
            } else if (err < 0) { /* some other error */
                _DEBUG("ERROR: SOMETHING IS WRONG, err: %d", err);
                return -6;
            } else { /* added to wnd correctly */
                _DEBUG("%s\n", "packet was added ...");
            }
        } else {
            _DEBUG("can't add to wnd with seq: %"PRIu32"\n", seq);
            free(pkt);
            return -1;
        }
    }
    else{
        _DEBUG("%s\n", "sending old data...");
    }

    do {
        err = (int)send(sockfd, pkt, DATA_OFFSET + datalen, 0);
    } while(errno == EINTR && err < 0);
    if(err < 0) {
        perror("xtcp.srvsend()");
        /* FIXME: fix this */
        _DEBUG("%s\n", "ERROR ERROR: DOING BAD THINGS!!!!!");
        /* remove_from_wnd((const char **)wnd); */
        free(pkt);
        return -2;
    }

    (*cli_wnd)--;
    print_wnd((const char**)wnd);

    return 1;
}

/**
* The client only directly calls this function once, for the SYN.
* It provides packet loss to the client's sends.
* This malloc()'s and free()'s space for data and a header.
*
*/
void clisend(int sockfd, uint32_t seq, uint32_t ack_seq, uint16_t flags, uint16_t adv_win, void *data, size_t datalen){
    void *pkt = alloc_pkt(seq, ack_seq, flags, adv_win, data, datalen);
    _DEBUG("%s\n", "printing hdr to send");
    print_hdr((struct xtcphdr*)pkt);
    htonpkt((struct xtcphdr*)pkt);

    clisend_lossy(sockfd, pkt, datalen);

    free(pkt);
}

void clisend_lossy(int sockfd, void *pkt, size_t datalen){
    ssize_t err;
    /* simulate packet loss on sends */
    if(drand48() >= pkt_loss_thresh) {
        err = send(sockfd, pkt, (DATA_OFFSET + datalen), 0);
        if (err < 0) {
            perror("xtcp.send_lossy()");
            exit(EXIT_FAILURE);
        }
    }
    else {
        printf("DROPPED SEND'ing PKT: ");
        ntohpkt((struct xtcphdr*)pkt);
        print_hdr((struct xtcphdr *) pkt);
    }
}

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
int clirecv(int sockfd, struct window* w) {
    ssize_t bytes = 0;
    uint32_t pktseq;
    int err;
    fd_set rset;
    char *pkt = malloc(MAX_PKT_SIZE + 1); /* +1 for consumer and printf */
    if(pkt == NULL){
        perror("clirecv().malloc()");
        exit(EXIT_FAILURE);
    }
    for(;;) {
        continue_with_select:
        /* todo: remove this print? */
        _DEBUG("%s\n", "select() for ever loop");
        print_window(w);
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);

        err = select(sockfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0){
            if(err == EINTR){
                continue;
            }
            perror("clirecv().select()");
            free(pkt);
            exit(EXIT_FAILURE);
        }
        if(FD_ISSET(sockfd, &rset)){
            /* recv the server's datagram */
            bytes = recv(sockfd, pkt, MAX_PKT_SIZE, 0);
            if(bytes < 0){
                perror("clirecv().recv()");
                free(pkt);
                return -2;
            }else if(bytes < 12){
                _DEBUG("clirecv().recv() not long enough to be pkt: %dbytes\n", (int)bytes);
                continue;
            }
            ntohpkt((struct xtcphdr *)pkt); /* host order please */
            pkt[bytes] = 0; /* NULL terminate the ASCII text */

            /* if it's a FIN or RST don't try to drop it, we're closing dirty! */
            if((((struct xtcphdr*)pkt)->flags & FIN) == FIN){
                _DEBUG("%s\n", "clirecv()'d a FIN packet.");
                free(pkt);
                return 0;
            }
            if((((struct xtcphdr*)pkt)->flags & RST) == RST){
                _DEBUG("%s\n", "clirecv()'d a RST packet!!! Server aborted connection!");
                free(pkt);
                return -1;
            }
            /* not a FIN: try to drop it */
            if(drand48() >= pkt_loss_thresh){
                /**
                * keep the pkt:
                * Pretend like the code after the "break;" is in here.
                * However, because it's not in here it will use a goto
                * instead of a continue later.
                */
                break;
            }else{
                /* drop the pkt */
                /* fixme: remove prints? */
                printf("DROPPED RECV'ing PKT: ");
                print_hdr((struct xtcphdr *) pkt);
                continue;
            }
        }
        /* end of select for(EVER) */
    }
    /* recv'ed a pkt, it is in host order, and we will put it in the window */
    printf("recv'd packet ");
    print_hdr((struct xtcphdr *) pkt);
    printf("packet contents:\n");
    printf("%s\n", pkt + DATA_OFFSET);

    /* do window stuff */
    pktseq = ((struct xtcphdr *)pkt)->seq;
    /* todo: use struct window *w */
    if(pktseq == ack_seq) {
        /* Send a new ack if it will fit in the window */
        add_to_wnd(ack_seq, pkt, w);
        advwin--;
        cli_ack(sockfd, w);
    }
    else if(pktseq < ack_seq){
        /* send a duplicate ACK */
        /* don't bother with window, it's in there */
        _DEBUG("%s\n", "duplicate packet, calling cli_dup_ack()");
        cli_dup_ack(sockfd, w);
        /* continue in this function!, pretend like this is a continue */
        goto continue_with_select;
    }
    else if(pktseq > ack_seq) {
        /* buffer out of order pkts */
        /* try to add to wnd, if it out of bounds then IGNORE, otherwise duplicate ACK */
        add_to_wnd(pktseq, pkt, w);
    }
    return -1;
}

void cli_ack(int sockfd, struct window* w) {
    /* update ack_seq, check to send a cumulative ACK */
    while(get_from_wnd(ack_seq, w) != NULL) {
        ++ack_seq;
    }
    printf("cli_ack(): sending normal ACK, ack_num: %"PRIu32"\n", ack_seq);
    clisend(sockfd, ACK, NULL, 0);
}

void cli_dup_ack(int sockfd, struct window* w) {
    printf("cli_dup_ack(): sending duplicate ACK, ack_num: %"PRIu32"\n", ack_seq);
    clisend(sockfd, ACK, NULL, 0);
}
