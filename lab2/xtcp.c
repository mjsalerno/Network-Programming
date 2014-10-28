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
    printf("|hdr| seq:%u", hdr->seq);
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
    printf(", advwin:%u\n", hdr->advwin);
}

void make_pkt(void *hdr, uint16_t flags, uint16_t advwin, void *data, size_t datalen) {
    struct xtcphdr *realhdr = (struct xtcphdr*)hdr;
    if(hdr == NULL){
        fprintf(stderr, "ERROR: make_pkt(): void *hdr == NULL\n");
        return;
    }
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
    if(data == NULL || datalen == 0){
        return;
    }
    memcpy(( (char*)(hdr) + DATA_OFFSET), data, datalen);
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

void print_wnd(const char** wnd) {
    int i;
    printf("wnd: ");
    for(i = 0; i < max_wnd_size; ++i) {
        if(wnd[i] != NULL) {
            printf("X ");
        } else {
            printf("_ ");
        }
    }
    printf("count: %d,  advwin: %" PRIu16 "\n", wnd_count, advwin);
}

/**
* RETURNS 0 if the wnd seems to be correct
*        -1 otherwise
*/
int print_wnd_check(const char **wnd){
    int x = 0, rtn = 0, numfound = 0;
    struct xtcphdr* pkt;
    _DEBUG("%s\n", "doing wnd check...");
    for(;x < max_wnd_size; x++){
        pkt = (struct xtcphdr*)wnd[wnd_base_i + x];
        if(pkt == NULL){
            continue;
        }else if(pkt->seq == (wnd_base_seq + x)){
            numfound++;
        }else{
            _DEBUG("BAD: wnd[%d]->seq is: %"PRIi32", expected: %"PRIi32"\n", wnd_base_i + x, pkt->seq, wnd_base_seq + x);
            rtn = -1;
        }
    }
    if(numfound != wnd_count){
        _DEBUG("BAD: found %d pkts in wnd, expected %d\n", numfound, wnd_count);/*fixme: rtn = -1 */
    }
    return rtn;
}

/**
* RETURNS: 1 if wnd_count == advwin (also when > advwin)
*
*/
int is_wnd_full(){
    int full = (wnd_count == advwin);
    if(wnd_count > advwin){
        full = 1;
        _DEBUG("ERROR: (wnd_count) %d > %d (max_wnd_size)\n", wnd_count, max_wnd_size);
    }
    return full;
}

/**
* RETURNS: 1 if count is = 0. (also when < 0)
*
*/
int is_wnd_empty(){
    int empty = (wnd_count == 0);
    if(wnd_count < 0){
        empty = 1;
        _DEBUG("ERROR: (wnd_count) %d < 0 \n", wnd_count);
    }
    return empty;
}

/**
* Calc the distance from the current wnd_base_seq.
* This gives the offset into the "continuous" buffer we have.
* MOD this only if it's smaller than the current advwin.
*/
int dst_from_base_wnd(uint32_t n) {
    int rtn = n - wnd_base_seq;
    _DEBUG("n: %-3"PRIu32" wnd_base_seq: %-3"PRIu32" max: %-3d dst: %-3d\n", n, wnd_base_seq, max_wnd_size, rtn);
    return rtn;
}

/**
* Pass in (pkt->ack_seq) - 1
* Returns 1 if ack_seq_1 >= wnd_base_seq
*         0 if not
*/
int ge_base(uint32_t ack_seq_1) {
    return (ack_seq_1 >= wnd_base_seq) && wnd_count > 0;
}


/**
* MUST have advwin set to the max wnd size.
*
* The arg first_seq_num must be the first index passed to add_to_wnd()
*
* RETURNS:
* A malloc()'d pointer to the window, to be passed to the window funcs.
* MUST be free_wnd()'d when done.
**/
char** init_wnd(uint32_t first_seq_num) {
    char** rtn;
    int i;
    wnd_count = 0;
    max_wnd_size = advwin;
    wnd_base_seq = first_seq_num;
    wnd_base_i = wnd_base_seq % max_wnd_size;

    _DEBUG("wnd_base_seq: %" PRIu32 ", wnd_base_i: %d max_wnd_size %d\n", wnd_base_seq, wnd_base_i, max_wnd_size);

    rtn = malloc((size_t)(max_wnd_size * sizeof(char*)));
    if(rtn == NULL){
        perror("init_wnd().malloc()");
        return NULL;
    }

    for(i = 0; i < advwin; ++i)
        *(rtn+i) = 0;

    return rtn;
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
    wnd_count = 0;
}

/**
* RETURNS: 1 if you can call add_to_wnd(seq, wnd)
*          0 otherwise
*/
int can_add_to_wnd(uint32_t seq){
    int can_add = 0;
    /* todo: add cwin to calc */
    if(wnd_base_seq <= seq && seq < (wnd_base_seq + max_wnd_size)){
        /* if it's valid */
        can_add = 1;
    }
    return can_add;
}

/**
* Adds packets that are already malloc()'d, into the window
* returns E_ISFULL       if can_add_to_wnd(index) returns 0
* returns E_OCCUPIED     if index was occupied
* returns 0 on success
*/
int add_to_wnd(uint32_t index, const char* pkt, const char** wnd) {
    int n = dst_from_base_wnd(index);
    _DEBUG("n = %d, going to ifs\n", n);
    if(!can_add_to_wnd(index)){
        _DEBUG("ERROR: can_add_to_wnd(%"PRIu32") == 0, can't add.\n", index);
        return E_ISFULL;
    }

    /* now we can mod by max_wnd_size */
    n = (n + wnd_base_i) % max_wnd_size;
    _DEBUG("(n + wnd_base_i) %% max_wnd_size = %d\n", n);

    if(wnd[n] != NULL) { /* sanity check */
        _DEBUG("wnd[%d] already ocupied, contained pkt->seq = %"PRIu32"\n", n, ((struct xtcphdr*)(wnd[n]))->seq);
        return E_OCCUPIED;
    }
    wnd[n] = pkt;
    wnd_count++;
    _DEBUG("added pkt at wnd[%d], wnd_count: %d\n", n, wnd_count);
    print_wnd(wnd);
    return 0;
    /* NOTE: don't try to update wnd_base_seq in here because the first
    * thing the client gets might not be the base of the wnd.
    * e.g. the first pkt recv'ed is the second pkt the server sent
    */
}

/**
* Removes the pkt pointer from the window at seq (index) and returns it.
*
* NOTE:
* Caller must free() the returned pointer.
*
*/
char* remove_from_wnd(const char** wnd) {
    const char* tmp;
    _DEBUG("removing wnd[wnd_base_i %d]\n", wnd_base_i);
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't remove because the wnd_count: %d\n", wnd_count);
        return NULL;
    }
    tmp = wnd[wnd_base_i];
    wnd[wnd_base_i] = NULL;
    wnd_count--;
    if(tmp == NULL){ /* sanity check */
        _DEBUG("%s\n","remove_from_wnd() returning NULL at wnd[wnd_base_i]");
    }
    /* this should be right, moving the wnd_base's forward on correct removals*/
    wnd_base_seq++;
    wnd_base_i++;
    if(wnd_base_i >= max_wnd_size){
        wnd_base_i -= max_wnd_size;
    }
    print_wnd(wnd);
    return (char*)tmp;
}

/**
* Same sorta stuff as remove except it doesn't remove, duh!
*
*/
char* get_from_wnd(uint32_t index, const char** wnd) {
    int n = dst_from_base_wnd(index);
    const char* tmp;

    /* change for congestion control? */
    if(n >= max_wnd_size) { /* sanity check */
        _DEBUG("you wanted index %"PRIu32", but it's beyond the window %d\n", index, max_wnd_size);
        return NULL;
    }
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't get, index %"PRIu32", the wnd_count: %d\n", index, wnd_count);
        return NULL;
    }

    /* now we can mod by max_wnd_size */
    n = (n + wnd_base_i) % max_wnd_size;
    _DEBUG("(n + wnd_base_i) %% max_wnd_size = %d\n", n);

    tmp = wnd[n];

    if(tmp == NULL){ /* sanity check */
        _DEBUG("get_from_wnd() is returning a NULL entry at n: %d\n", n);
    }
    return (char*)tmp;
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
    ssize_t err;
    void *pkt = malloc(DATA_OFFSET + datalen);
    if(pkt == NULL){
        perror("clisend().malloc()");
        return -1;
    }

    make_pkt(pkt, flags, advwin, data, datalen);
    _DEBUG("%s\n", "printing hdr to send");
    print_hdr((struct xtcphdr*)pkt);
    htonpkt((struct xtcphdr*)pkt);

    /* simulate packet loss on sends */
    if(drand48() >= pkt_loss_thresh) {
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
    }

    free(pkt);
    return 0;
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
                _DEBUG("clirecv().recv() not long enough to be pkt: %d\n", bytes);
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
                printf("DROPPED PKT: ");
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
        _DEBUG("%s\n", "pktseq: "PRIu32" == "PRIu32" :ack_seq, good but check wnd not full", pktseq, ack_seq);
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
