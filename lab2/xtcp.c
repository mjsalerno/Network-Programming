#include <errno.h>
#include "xtcp.h"

static int max_wnd_size;   /* initialized by init_wnd() */

/* the seq number mapping to the current wnd base */
static uint32_t wnd_base_seq; /* initialized by init_wnd() */
static int wnd_base_i; /* initialized by init_wnd() */
static int wnd_count = 0; /* number of pkts currently in the window */

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

struct win_node* alloc_window_nodes(size_t n) {
    struct win_node* head;
    struct win_node* ptr;

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
    ptr = head->next;

    for(n -= 1; n > 0; --n) {
        ptr = malloc(sizeof(struct win_node));
        if(ptr == NULL) {
            perror("alloc_window(): malloc failed");
            exit(EXIT_FAILURE);
        }
        ptr->datalen = -1;
        ptr->pkt = NULL;
        ptr = ptr->next;
    }

    ptr->next = head;
    return head;
}

struct window* init_window(int maxsize, uint32_t srv_last_seq_sent, uint32_t srv_last_ack_seq_recvd,
        uint32_t cli_top_accept_seqn, uint32_t cli_last_seqn_recvd){

    struct window* w;
    w = malloc(sizeof(struct window));
    if(w == NULL){
        perror("ERROR init_window().malloc()");
        exit(EXIT_FAILURE);
    }
    w->maxsize = maxsize;
    w->servlastackrecv = srv_last_ack_seq_recvd;
    w->servlastpktsent = srv_last_seq_sent;
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
* srv_send_recv(){
*
*
*   send until full
*
*   recv with BLOCK
*   recv with MSG_DONTWAIT until you get EWOULDBLOCK
*       check 
*
*
*/

void free_window(struct win_node* head) {
    struct win_node* ptr1;
    struct win_node* ptr2;
    ptr2 = head->next;
    ptr1 = ptr2->next;

    for(; ptr2 != head; ptr2 = ptr1, ptr1=ptr1->next) {
        free(ptr2);
    }

    free(head);
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

/**
*
*/
void ackrecvd(struct window *window, struct xtcphdr *pkt){
    if(window->cwin < window->ssthresh) {
        /* slow start */
        window->cwin = window->cwin + 1;
    }
    else {
        /** todo: congestion cntrl
        *  count numacks
        *  if numacks == cwin
        *  then cwin++, num acks = 0
        **/
        window->cwin = window->cwin; /*    +1/cwin     */
    }
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
* RETURNS: -1 on failure
*           0 on success
*
*/
int clisend(int sockfd, uint16_t flags, void *data, size_t datalen){
    void *pkt = malloc(DATA_OFFSET + datalen);
    if(pkt == NULL){
        perror("clisend().malloc()");
        exit(EXIT_FAILURE);
    }

    make_pkt(pkt, flags, advwin, data, datalen);
    _DEBUG("%s\n", "printing hdr to send");
    print_hdr((struct xtcphdr*)pkt);
    htonpkt((struct xtcphdr*)pkt);

    clisend_lossy(sockfd, pkt, datalen);

    free(pkt);
    return 0;
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


int clirecv(int sockfd, char **wnd) {
    ssize_t bytes = 0;
    uint32_t pktseq;
    int err;
    fd_set rset;
    char *pkt = malloc(MAX_PKT_SIZE + 1); /* +1 for consumer and printf */
    if(pkt == NULL){
        perror("clirecv().malloc()");
        return -1;
    }
    for(;;) {
        continue_with_select:
        /* todo: remove this print? */
        _DEBUG("%s\n", "select() for ever loop");
        print_wnd((const char **)wnd);
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);

        err = select(sockfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0){
            if(err == EINTR){
                continue;
            }
            perror("clirecv().select()");
            free(pkt);
            return -2;
        }
        if(FD_ISSET(sockfd, &rset)){
            /* recv the server's datagram */
            bytes = recv(sockfd, pkt, MAX_PKT_SIZE, 0);
            if(bytes < 0){
                perror("clirecv().recv()");
                free(pkt);
                return -2;
            }else if(bytes < 12){
                _DEBUG("clirecv().recv() not long enough to be pkt: %d\n", (int)bytes);
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
    if(pktseq == ack_seq) {
        /* everythings going fine, send a new ACK */
        _DEBUG("pktseq: %" PRIu32 " == %" PRIu32 " :ack_seq, good but check wnd not full\n", pktseq, ack_seq);
        err = add_to_wnd(ack_seq, pkt, (const char**)wnd);
        if(err < 0){
            _DEBUG("just ignore pkt, wnd must be full add_to_wnd() returned %d \n", err);
            goto continue_with_select;
        }
        advwin--;
        err = cli_ack(sockfd, wnd);
        if(err < 0){
            _DEBUG("cli_ack() returned %d\n", err);
            return -2;
        }
        /* the normal return value */
        return (int)bytes;
    }
    else if(pktseq < ack_seq){
        /* send a duplicate ACK */
        /* don't bother with window, it's in there */
        _DEBUG("%s\n", "duplicate packet, calling cli_dup_ack()");
        err = cli_dup_ack(sockfd);
        if(err < 0){
            _DEBUG("cli_dup_ack() returned %d\n", err);
            return -2;
        }
        /* continue in this function!, pretend like this is a continue */
        goto continue_with_select;
    }
    else if(pktseq > ack_seq) {
        /* try to add to wnd, if it out of bounds then IGNORE, otherwise duplicate ACK */
        err = add_to_wnd(pktseq, pkt, (const char **) wnd);
        _DEBUG("add_to_wnd() returned: %d\n", err);
        switch (err) {
            case 0:
                /* send ACK, added correctly and for the first time */
                _DEBUG("%s\n", "buffered new gap packet, calling cli_dup_ack()");
                advwin--;
                err = cli_dup_ack(sockfd);
                if (err < 0) {
                    return -2;
                }
                return (int)bytes;
            case E_OCCUPIED:
                /* send duplicate ACK */
                _DEBUG("%s\n", "duplicate gap packet, calling cli_dup_ack()");
                err = cli_dup_ack(sockfd);
                if (err < 0) {
                    return -2;
                }
                goto continue_with_select;
            case E_ISFULL:
                /* don't ACK this, it's too far */
                printf("not ACKing pkt because I can't store it! Just ignore it!\n");
                goto continue_with_select;
            default:
                _DEBUG("ERROR: %d SOMETHING IS WRONG WITH WINDOW\n", err);
                return -2;
        }
    }
    /* should never get here */
    assert(0 == 1);
    return -5;
}

int cli_ack(int sockfd, char **wnd) {
    int err;
    /* update ack_seq, check to send a cumulative ACK */
    while(get_from_wnd(ack_seq, (const char**)wnd) != NULL) {
        ++ack_seq;
    }
    printf("cli_ack(): sending normal ACK, ack_num: %"PRIu32"\n", ack_seq);
    err = clisend(sockfd, ACK, NULL, 0);

    return (int)err;
}

int cli_dup_ack(int sockfd) {
    int err;
    printf("cli_dup_ack(): sending duplicate ACK, ack_num: %"PRIu32"\n", ack_seq);
    err = clisend(sockfd, ACK, NULL, 0);

    return err;
}
