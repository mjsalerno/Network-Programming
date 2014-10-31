#include "xtcp.h"

pthread_mutex_t w_mutex;

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
* returns in HOST order
*/
void *alloc_pkt(uint32_t seqn, uint32_t ack_seqn, uint16_t flags, uint16_t adv_win, void *data, size_t datalen) {
    struct xtcphdr *pkt;
    if(data == NULL || datalen <= 0){
        datalen = 0;
    }
    pkt = malloc(DATA_OFFSET + datalen);
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
    int n = -1;
    n++; /* for -pedantic -Wextra */
    printf("|window| maxwnd: %5d, cwin: %5d, ssthresh: %5d, base: %p\n",
            w->maxsize, w->cwin, w->ssthresh, (void*)w->base);

    do {
        if(curr->datalen < 0) {
            printf("_ ");
        }else {
            printf("X ");
        }
        curr = curr->next;
    } while(curr != head);
    printf("\n");

    #ifdef DEBUG
    printf("DEBUG |window| nodes:\n");
    do {
        printf("========= node %3d ========\n", n);
        printf("|Datalen: %15d |\n", curr->datalen);
        printf("|Pkt ptr: %15p |\n", (void *)curr->pkt);
        if(curr->pkt != NULL) {
            printf("|Pkt seq: %15"PRIu32" |\n", curr->pkt->seq);
        }else{
            printf("|Pkt seq:          (none) |\n");
        }
        printf("|This:    %15p |\n", (void *)curr);
        printf("|Next:    %15p |\n", (void *)curr->next);
        printf("========= node %3d ========\n", n);
        curr = curr->next;
        n++;
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

void free_window(struct window* wnd) {
    struct win_node* head = wnd->base;
    struct win_node* tail = head->next;
    struct win_node* tmp;

    while(tail != head) {
        tmp = tail;
        tail = tail->next;
        free(tmp);
    }

    free(head);
    free(wnd);
}

/**
* Called to init the sliding window.
*/
struct window* init_window(int maxsize, uint32_t srv_last_seq_sent, uint32_t srv_last_ack_seq_recvd,
        uint32_t cli_last_seqn_recvd, uint32_t cli_base_seqn) {

    struct window* w;
    w = malloc(sizeof(struct window));
    if(w == NULL) {
        perror("ERROR init_window().malloc()");
        exit(EXIT_FAILURE);
    }

    w->maxsize = maxsize;
    w->numpkts = 0;
    w->lastadvwinrecvd = maxsize;
    w->servlastackrecv = srv_last_ack_seq_recvd;
    w->servlastseqsent = srv_last_seq_sent;
    /* todo: change to 1 */
    w->cwin = 65536;
    w->ssthresh = 65536;
    w->dupacks = 0;
    w->clibaseseq = cli_base_seqn;
    w->clilastunacked = cli_last_seqn_recvd;
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
    htonpkt(w->base->pkt);
    err = send(sockfd, w->base->pkt , (size_t)w->base->datalen, 0);
    if(err < 0){
        perror("ERROR srv_send_base().send()");
        exit(EXIT_FAILURE);
    }
    ntohpkt(w->base->pkt);
}

/**
* todo: Please mask sigalrm, sigprocmask()
* Give me pkt in host order!! The window contains host order packets.
*
* Prints the window and the header sent.
*
*/
void srv_add_send(int sockfd, void* data, size_t datalen, uint16_t flags, struct window *w) {
    struct win_node *base;
    struct win_node *curr;
    int n = 0;
    int effectivesize;
    uint32_t seqtoadd;
    void* pkt;

    effectivesize = MIN(w->cwin, MIN(w->lastadvwinrecvd, w->maxsize));

    if(w == NULL) {
        _ERROR("%s\n", "srv_add_send() w is NULL!\n");
        exit(EXIT_FAILURE);
    }
    if(data == NULL) {
        _ERROR("%s\n", "srv_add_send() data is NULL, can't add/send NULL!\n");
        exit(EXIT_FAILURE);
    }
    base = w->base;
    if(base == NULL) {
        _ERROR("%s\n", "srv_add_send() w->base is NULL!\n");
        exit(EXIT_FAILURE);
    }

    if(effectivesize <= 0) {
        _ERROR("srv_add_send() effective winsize: %d is <= 0\n", effectivesize);
        print_window(w);
        exit(EXIT_FAILURE);
    }

    pkt = alloc_pkt(w->servlastseqsent + 1, 0, flags, 0, data, datalen);
    print_hdr(pkt);

    seqtoadd = ((struct xtcphdr*) pkt)->seq;
    if(seqtoadd != (w->servlastseqsent + 1)){
        fprintf(stderr, "ERROR: srv_add_send() seqtoadd: %"PRIu32" != "
                        "(w->servlastseqsent + 1): %"PRIu32"\n", seqtoadd,
                (w->servlastseqsent + 1));
        print_window(w);
        exit(EXIT_FAILURE);
    }
    curr = base;
    while(n < effectivesize) {
        if(seqtoadd == (n + w->servlastackrecv)) {
            /* we are at the correct node */
            break;
        }
        /* inc n and move curr to the next*/
        n++;
        curr = curr->next;
        if(curr == NULL) {
            _ERROR("%s\n", "srv_add_send() a win_node is NULL, init your window!\n");
            print_window(w);
            exit(EXIT_FAILURE);
        }
        if(curr == base) {
            _ERROR("%s\n", "srv_add_send() reached the base while trying to add to window!\n");
            print_window(w);
            exit(EXIT_FAILURE);
        }
    }
    /* curr is now at the win_node we want? */
    if(curr->datalen >= 0 || curr->pkt != NULL) {
        _ERROR("%s\n", "srv_add_send() curr->pkt not empty!\n");
        print_window(w);
        exit(EXIT_FAILURE);
    } else {
        _DEBUG("%s", "Found win_node for pkt. Adding to window.\n");
        curr->datalen = (int)datalen;
        curr->pkt = pkt;
        w->servlastseqsent = seqtoadd;
    }

    htonpkt(pkt);
    n = (int) send(sockfd, pkt, (size_t)datalen, 0);
    if(n < 0) {
        _ERROR("%s\n", "srv_add_send().send()");
        exit(EXIT_FAILURE);
    }
    w->servlastseqsent = w->servlastseqsent + 1;
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
        _DEBUG("we are in slow start cwin: %d, ssthresh: %d", window->cwin, window->ssthresh);
        window->cwin = window->cwin + 1;
        count = remove_aked_pkts(window, pkt);
        _DEBUG("number of ACKs: %d\n", count);
        window->cwin += count;
        _DEBUG("new cwin: %d\n", window->cwin);
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
        _NOTE("got dup ack: %" PRIu32 "\n", pkt->ack_seq);
        window->dupacks++;
        _NOTE("new dupack: %" PRIu32 "\n", window->dupacks);
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

void fast_retransmit(struct window* w) {
    _NOTE("%s\n", "Sending Fast Retransmit");
    w->ssthresh = MAX(((w->servlastseqsent - w->base->pkt->seq)/ 2), 2);
    /* todo: finish me*/
}

/**
* pkt in HOST order please
*/
void clisend_lossy(int sockfd, struct xtcphdr *pkt, size_t datalen) {
    ssize_t err;
    /* simulate packet loss on sends */
    if(DROP_PKT()) {
        _NOTE("%s", "DROPPED SEND'ing PKT: ");
    } else {
        htonpkt(pkt);
        err = send(sockfd, pkt, (DATA_OFFSET + datalen), 0);
        if (err < 0) {
            perror("xtcp.send_lossy()");
            exit(EXIT_FAILURE);
        }
        ntohpkt(pkt);
        _NOTE("%s", "SENT PKT: ");
    }
    print_hdr(pkt);
}


/**
* Give pkt in host order.
*
* RETURNS: 0 if
*/
int cli_add_send(int sockfd, struct xtcphdr *pkt, int datalen, struct window* w) {
    struct win_node *base;
    struct win_node *curr;
    uint32_t seqtoadd;
    struct xtcphdr *ackpkt;
    int gaplength = 0;

    get_lock(&w_mutex);

    if(w == NULL) {
        _ERROR("%s\n", "w is NULL!");
        exit(EXIT_FAILURE);
    }
    if(pkt == NULL) {
        _ERROR("%s\n", "data is NULL, can't add/ack NULL!");
        exit(EXIT_FAILURE);
    }
    base = w->base;
    if(base == NULL) {
        _ERROR("%s\n", "w->base is NULL!");
        exit(EXIT_FAILURE);
    }

    /* recv'ed a pkt, it is in host order, and we will put it in the window */
    printf("recv'd packet ");
    print_hdr(pkt);
    /* todo: remove */
    printf("packet contents:\n");
    printf("%s\n", (char*)pkt + DATA_OFFSET);


    seqtoadd = pkt->seq;
    _DEBUG("Trying to add pkt: %"PRIu32" to window...\n", seqtoadd);

    curr = get_node(seqtoadd, w);
    if(curr != NULL) {

        _DEBUG("%s", "found win_node to add pkt\n");

        if (curr->datalen > 0 && curr->pkt != NULL) { /* win_node is occupied */
            if (seqtoadd != curr->pkt->seq) {
                _ERROR("%s\n", "found win_node occupied with different seq!");
                print_window(w);
                exit(EXIT_FAILURE);
            } else {
                _DEBUG("%s\n", "found win_node already had the seqtoadd");
                /* ack this will become a dup ack*/
            }
        } else { /* win_node is empty*/
            /* put the pkt in there */
            curr->datalen = datalen;
            curr->pkt = pkt;
            w->numpkts = w->numpkts + 1;
        }


        /* if it filled a gap then try to make a cumulative ACK by looking at the next pkts */
        if (seqtoadd == w->clilastunacked) {
            _DEBUG("%s\n", "looking if the pkt filled a gap....");
            curr = curr->next;
            while (curr != base) {
                gaplength++;
                if (curr->pkt == NULL) {
                    _DEBUG("win_node #%d was empty, stopping search", (w->clibaseseq - seqtoadd) + gaplength);
                    break;
                }
                _DEBUG("win_node #%d had pkt! continue search", (w->clibaseseq - seqtoadd) + gaplength);
                curr = curr->next;
            }
        }
    }

    /* make pkt with maxsize - numpkts */
    /* fixme: seq num of acks */
    ackpkt = alloc_pkt(111111, (w->clilastunacked + gaplength), ACK, (uint16_t)(w->maxsize - w->numpkts), NULL, 0);
    unget_lock(&w_mutex);

    clisend_lossy(sockfd, ackpkt, sizeof(struct xtcphdr));
    free(ackpkt);
    return 0;
}


/**
* Finds the node where seqtoget should go and returns it.
* Returns NULL if not in the window
*/
struct win_node* get_node(uint32_t seqtoget, struct window *w) {
    struct win_node* base;
    struct win_node* curr;
    int n = 0;
    if(w == NULL) {
        _ERROR("%s\n", "w is NULL!");
        exit(EXIT_FAILURE);
    }
    base = w->base;

    if(base == NULL) {
        _ERROR("%s\n", "w->base is NULL!");
        exit(EXIT_FAILURE);
    }

    /* below window              or             above window */
    if(seqtoget < w->clibaseseq  || seqtoget >= (w->clibaseseq + w->maxsize)) {
        _DEBUG("Returning NULL, %"PRIu32" is outside the window\n", seqtoget);
        return NULL;
    }

    _DEBUG("Looking for win_node to grab %"PRIu32"...\n", seqtoget);

    curr = base;
    do {
        _DEBUG("looking at win_node #%d ...\n", n);
        if(seqtoget ==  (w->clibaseseq + n)) {
            _DEBUG("Found %"PRIu32" in win_node #%d\n", seqtoget, n);
            return curr;
        }

        if(curr == NULL){
            _ERROR("win_node #%d is NULL, init your window!\n", n);
            print_window(w);
            exit(EXIT_FAILURE);
        }
        curr = curr->next;
        n++;
    } while(curr != base); /* stop if we wrap around */

    _ERROR("Shouldn't reach this spot! seqtoget: %"PRIu32", "
            "clibaseseq: %"PRIu32", maxsize: %d\n", seqtoget, w->clibaseseq, w->maxsize);
    return NULL; /* we wrapped around */
}

void get_lock(pthread_mutex_t* lock) {
    int err;
    err = pthread_mutex_lock(lock);
    if(err > 0) {
        errno = err;
        perror("CONSUMER: ERROR pthread_mutex_lock()");
        exit(EXIT_FAILURE);
    }
    _INFO("%s\n", "Locked the window.");
}

void unget_lock(pthread_mutex_t* lock) {
    int err;
    err = pthread_mutex_unlock(lock);
    if(err > 0) {
        errno = err;
        perror("CONSUMER: ERROR pthread_mutex_unlock()");
        exit(EXIT_FAILURE);
    }
    _INFO("%s\n", "Released the window.");
}
