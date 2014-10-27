#include <inttypes.h>
#include <errno.h>
#include "xtcp.h"
#include "debug.h"

static int max_wnd_size;   /* initialized by init_wnd() */

/* the seq number mapping to the current wnd base */
static uint32_t wnd_base_seq; /* initialized by init_wnd() */
static int wnd_base_offset; /* initialized by init_wnd() */
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
    printf("wnd: count: %d ", wnd_count);
    for(i = 0; i < max_wnd_size; ++i) {
        if(wnd[i] != NULL) {
            printf("X ");
        } else {
            printf("_ ");
        }
    }
    printf("\n");
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

int has_packet(uint32_t index, const char** wnd) {
    int n = dst_from_base_wnd(index);
    /* now we can mod by max_wnd_size */
    n = n % max_wnd_size;

    return wnd[n] != NULL;
}

/**
* Calc the distance from the current wnd_base_seq.
* This gives the offset into the "continuous" buffer we have.
* MOD this only if it's smaller than the current advwin.
*/
int dst_from_base_wnd(uint32_t n) {
    int rtn = n - wnd_base_seq;
    _DEBUG("n: %-3" PRIu32 " wnd_base_seq: %-3d max: %-3d dst: %-3d\n", n, wnd_base_seq, max_wnd_size, rtn);
    return rtn;
}

/**
* Pass in (pkt->ack_seq) - 1
* Returns 1 if ack_seq_1 >= wnd_base_seq
*         0 if not
*/
int ge_base(uint32_t ack_seq_1){
    return (ack_seq_1 >= wnd_base_seq);
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
char** init_wnd(int first_seq_num) {
    char** rtn;
    int i;
    wnd_count = 0;
    max_wnd_size = advwin;
    wnd_base_seq = first_seq_num;
    wnd_base_offset = wnd_base_seq % max_wnd_size;

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
* Adds packets that are already malloc()'d to the window
* returns E_WASREMOVED   index used to be in the wnd, i.e. index < wnd_base_seq
* returns E_CANTFIT      index cannot currently fit into the wnd, i.e. wnd_base_seq-index > advwin
* returns E_INDEXTOOFAR  index would exceed the max_wnd_size
* returns E_OCCUPIED     if index was occupied
* returns 0 on success fully added
*/
int add_to_wnd(uint32_t index, const char* pkt, const char** wnd) {
    int n = dst_from_base_wnd(index);
    _DEBUG("n = %d, going to ifs\n", n);

    if(n < 0){
        _DEBUG("index: %"PRIu32" used to be in the wnd, n: %d\n", index, n);
        return E_WASREMOVED;
    }
    else if(n > max_wnd_size){
        _DEBUG("index %"PRIu32" tried to exceed max wnd size, n: %d\n", index, n);
        return E_INDEXTOOFAR;
    }
    else if(n > advwin) {
        _DEBUG("index %"PRIu32" cannot currently fit into the wnd, n: %d\n", index, n);
        return E_CANTFIT;
    }

    /*
    if(max_wnd_size == wnd_count){
        _DEBUG("window is full but this index passed: %"PRIu32"\n", index);
        return -6;
    }
    */

    /* now we can mod by max_wnd_size */
    n = (n + wnd_base_offset) % max_wnd_size;
    _DEBUG("n %% max_wnd_size = %d\n", n);

    if(wnd[n] != NULL) { /* sanity check */
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() index location already ocupied: %d\n", n);
        return E_OCCUPIED;
    }
    wnd[n] = pkt;
    wnd_count++;
    _DEBUG("added pkt at wnd[%d], wnd_count: %d\n", n, wnd_count);
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
    _DEBUG("removing wnd[wnd_base_offset %d]\n", wnd_base_offset);
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't remove because the wnd_count: %d\n", wnd_count);
        return NULL;
    }
    tmp = wnd[wnd_base_offset];
    wnd[wnd_base_offset] = NULL;
    wnd_count--;
    if(tmp == NULL){ /* sanity check */
        _DEBUG("%s\n","remove_from_wnd() returning NULL at wnd[wnd_base_offset]");
    }
    /* this should be right, moving the wnd_base's forward on correct removals*/
    wnd_base_seq++;
    wnd_base_offset++;
    if(wnd_base_offset >= max_wnd_size){
        wnd_base_offset -= max_wnd_size;
    }
    return (char*)tmp;
}

/**
* Same sorta stuff as remove except it doesn't remove, duh!
*
*/
char* get_from_wnd(uint32_t index, const char** wnd) {
    int n = dst_from_base_wnd(index);
    const char* tmp;

    if(n > advwin) {
        /* change for congestion control? */
        fprintf(stderr, "ERROR: can't get, index %d is beyond advwin %d\n", (int)index, advwin);
        return NULL;
    }
    if(n > max_wnd_size) { /* sanity check */
        fprintf(stderr, "ERROR: can't get, index %d is beyond the max window size %d\n", (int)index, max_wnd_size);
        return NULL;
    }
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't get, index %d, the wnd_count: %d\n", (int)index, wnd_count);
        return NULL;
    }

    /* now we can mod by max_wnd_size */
    n = (n + wnd_base_offset) % max_wnd_size;

    tmp = wnd[n];

    if(tmp == NULL){ /* sanity check */
        _DEBUG("get_from_wnd() is returning a NULL entry at n: %d\n", n);
    }
    return (char*)tmp;
}


int srvsend(int sockfd, uint16_t flags, void *data, size_t datalen, char** wnd) {

    int err;
    void *pkt = malloc(sizeof(struct xtcphdr) + datalen);
    make_pkt(pkt, flags, advwin, data, datalen);
    /*print_hdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

    /*todo: do WND/rtt stuff*/
    err = add_to_wnd(seq, pkt, (const char**)wnd);
    switch(err) {
        case E_OCCUPIED:
        case E_CANTFIT:
        case E_INDEXTOOFAR:
            _DEBUG("%s\n", "The window was full, not sending");
            free(pkt);
            return -1;
        case -100:
            _DEBUG("ERROR: %s\n", "out of bounds");
            free(pkt);
            return -3;
        case 0:
            _DEBUG("%s\n", "packet was added ...");
            break;
        case E_WASREMOVED:
        default:
            _DEBUG("ERROR: SOMETHING IS WRONG: %d\n", err);
            free(pkt);
            return -5;
    }

    err = (int)send(sockfd, pkt, DATA_OFFSET + datalen, 0);
    if(err < 0) {
        perror("xtcp.srvsend()");
        remove_from_wnd(seq, (const char **)wnd);
        free(pkt);
        return -2;
    }

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
    /*print_hdr((struct xtcphdr*)pkt);*/
    htonpkt((struct xtcphdr*)pkt);

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
            }
            ntohpkt((struct xtcphdr *)pkt); /* host order please */
            pkt[bytes] = 0; /* NULL terminate the ASCII text */

            /* if it's a FIN don't try to drop it, we're closing dirty! */
            if((((struct xtcphdr*)pkt)->flags & FIN) == FIN){
                free(pkt);
                return 0;
            }
            /* not a FIN: try to drop it */
            if(drand48() > pkt_loss_thresh){
                /* keep the pkt */
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
    /* if it's a RST then return */
    if((((struct xtcphdr*)pkt)->flags & RST) == RST){
        _DEBUG("%s\n", "clirecv()'d a RST packet!!! Server aborted connection!");
        free(pkt);
        return -1;
    }

    print_hdr((struct xtcphdr *) pkt);
    printf("packet contents:\n");
    printf("%s\n", pkt + DATA_OFFSET);

    /* do window stuff */
    pktseq = ((struct xtcphdr *)pkt)->seq;
    if(pktseq == ack_seq){ /* everythings going fine */
        err = add_to_wnd(ack_seq, pkt, (const char**)wnd);
    }

    _DEBUG("%s\n", "placing packet into the window.");
    err = add_to_wnd(((struct xtcphdr *)pkt)->seq, pkt, (const char**)wnd);
    _DEBUG("add_to_wnd() returned: %d\n", err);
    switch(err){
        case 0:
            /* send ACK, added correctly and for the first time */
            err = cli_ack(sockfd, wnd);
            if(err < 0){
                return -2;
            }
            break;
        case E_WASREMOVED:
            /* send duplicate ACK */
            err = cli_dup_ack(sockfd, wnd);
            if(err < 0){
                return -2;
            }
            break;
        case E_OCCUPIED:
            /* send duplicate ACK */
            err = cli_dup_ack(sockfd, wnd);
            if(err < 0){
                return -2;
            }
            break;
        case E_INDEXTOOFAR:
            /* fixme: why don't ACK, instead does this mean bad flow control? */
            /* don't ACK this, it's too far */
            printf("not acking prev hdr because flow control bad? Plus I can't store it.\n");
            break;
        case E_CANTFIT:
            /* i don't know */
            break;
        default:
            _DEBUG("ERROR: %d SOMETHING IS WRONG WITH WINDOW\n", err);
    }
    return (int)bytes;
}

int cli_ack(int sockfd, char **wnd) {
    int err;
    /* update ack_seq, check to send a cumulative ACK */
    /* todo: check this????? */
    ++ack_seq;
    while(get_from_wnd(ack_seq, (const char**)wnd) != NULL) {
        ++ack_seq;
    }
    err = clisend(sockfd, ACK, NULL, 0);
    return (int)err;
}

int cli_dup_ack(int sockfd, char **wnd) {
    int err;
    /*todo: window stuff*/
    wnd++;
    err = clisend(sockfd, ACK, NULL, 0);
    return err;
}
