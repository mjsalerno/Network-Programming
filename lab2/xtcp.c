#include <inttypes.h>
#include <errno.h>
#include "xtcp.h"
#include "debug.h"

static int max_wnd_size;   /* initialized by init_wnd() */

/* the seq number mapping to the current wnd base */
static int wnd_base_seq; /* initialized by init_wnd() */
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
    realhdr->seq = seq;
    realhdr->flags = flags;
    realhdr->ack_seq = ack_seq;
    realhdr->advwin = advwin;
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
    printf("count: %d", wnd_count);
    for(i = 0; i < max_wnd_size; ++i) {
        if(has_packet((uint32_t)i, wnd)) {
            printf("X ");
        } else {
            printf("_ ");
        }
    }
    printf("\n");
}

int has_packet(uint32_t index, const char** wnd) {
    int n = dst_from_base_wnd(index);
    return wnd[n] != NULL;
}

/**
* Calc the distance from the current wnd_base_seq.
* This gives the offset into the "continuous" buffer we have.
* MOD this only if it's smaller than the current advwin.
*/
int dst_from_base_wnd(uint32_t n) {
    int rtn = n - wnd_base_seq;
    _DEBUG("n: %-3" PRIu32 " basewin: %-3d max: %-3d got index: %-3d\n", n, wnd_base_seq, max_wnd_size, rtn);
    return rtn;
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

    rtn = malloc((size_t)(max_wnd_size * sizeof(char*)));

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
* returns -4 index used to be in the wnd, i.e. index < wnd_base_seq
* returns -3 index cannot currently fit into the wnd, i.e. wnd_base_seq-index > advwin
* returns -2 index would exceed max wnd size
* returns -1 if window is full
* returns 0 if index was occupied
* returns 1 on success fully added
*/
int add_to_wnd(uint32_t index, const char* pkt, const char** wnd) {
    int n = dst_from_base_wnd(index);

    if(n < 0){
        _DEBUG("index %"PRIu32" used to be in the wnd ", index);
        return -4;
    }
    else if(n > max_wnd_size){
        _DEBUG("index %"PRIu32" tried to exceed max wnd size\n", index);
        return -2;
    }
    else if(n > advwin) {
        _DEBUG("index %"PRIu32" cannot currently fit into the wnd\n", index);
        return -3;
    }

    if(max_wnd_size == wnd_count){ /* sanity check */
        _DEBUG("window is full but this index passed: %"PRIu32"\n", index);
        return -1;
    }

    /* now we can mod by max_wnd_size */
    n = n % max_wnd_size;

    if(wnd[n] != NULL) { /* sanity check */
        fprintf(stderr, "ERROR: xtcp.add_to_wnd() index location already ocupied: %d\n", n);
        return 0;
    }
    wnd[n] = pkt;
    wnd_count++;
    _DEBUG("fine, stored at n: %d, new wnd_count: %d", n, wnd_count);
    return 1;
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
char* remove_from_wnd(uint32_t index, const char** wnd) {
    int n = dst_from_base_wnd(index);
    const char* tmp;

    if(n > advwin) {
        fprintf(stderr, "ERROR: can't remove, index %"PRIu32" is beyond advwin %d\n", index, advwin);
        return NULL;
    }
    if(n > max_wnd_size) { /* sanity check */
        fprintf(stderr, "ERROR: can't remove, index %"PRIu32" is beyond the max window size %d\n", index, max_wnd_size);
        return NULL;
    }
    if(n != 0) { /* sanity check, users only remove the base */
        fprintf(stderr, "ERROR: can't remove, index %"PRIu32" != wnd_base_seq %d\n", index, wnd_base_seq);
        return NULL;
    }
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't remove, index %"PRIu32", the window count:", wnd_count);
        return NULL;
    }

    /* now we can mod by max_wnd_size */
    n = n % max_wnd_size;

    tmp = wnd[n];
    wnd[n] = NULL;
    wnd_count--;
    /* this should be right, moving the wnd_base_seq forward on correct removals*/
    wnd_base_seq = index;

    if(tmp == NULL){ /* sanity check */
        _DEBUG("remove_from_wnd() is returning a NULL entry at n: %d\n", n);
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
        fprintf(stderr, "ERROR: can't get, index %"PRIu32" is beyond advwin %d\n", index, advwin);
        return NULL;
    }
    if(n > max_wnd_size) { /* sanity check */
        fprintf(stderr, "ERROR: can't get, index %"PRIu32" is beyond the max window size %d\n", index, max_wnd_size);
        return NULL;
    }
    if(wnd_count <= 0) { /* sanity check, should never happen */
        fprintf(stderr, "ERROR: can't get, index %"PRIu32", the window count:", wnd_count);
        return NULL;
    }

    /* now we can mod by max_wnd_size */
    n = n % max_wnd_size;

    tmp = wnd[n];

    if(tmp == NULL){ /* sanity check */
        _DEBUG("get_from_wnd() is returning a NULL entry at n: %d\n", n);
    }
    return (char*)tmp;
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

    print_wnd((const char**)wnd);

    return 1;
}

/*
The client only directly calls this function once, for the SYN.
It provides packet loss to the client's sends.
This malloc()'s and free()'s space for data and a header.
*/
int clisend(int sockfd, uint16_t flags, void *data, size_t datalen){
    ssize_t err;
    void *pkt = malloc(DATA_OFFSET + datalen);

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
    return 1;
}


int clirecv(int sockfd, char **wnd) {
    ssize_t bytes = 0;
    int err;
    fd_set rset;
    char *pkt = malloc(MAX_PKT_SIZE + 1); /* +1 for consumer and printf */
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
            /* if not a FIN: try to drop it */
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
    print_hdr((struct xtcphdr *) pkt);

    /* if it's a RST then return */
    if((((struct xtcphdr*)pkt)->flags & RST) == RST){
        _DEBUG("%s\n", "clirecv()'d a RST packet!!! Server aborted connection!");
        free(pkt);
        return -1;
    }
    _DEBUG("%s\n", "placing packet into the window.");
    err = add_to_wnd(((struct xtcphdr *)pkt)->seq, pkt, (const char**)wnd);
    if(err == -1) {          /* out of bounds */
        _DEBUG("ERROR: %s\n", "out of bounds");
        free(pkt);
        return -2;

    } else if(err == -2) {  /* full window */
        _DEBUG("%s\n", "The window was full, not sending");
        free(pkt);
        return -1;

    } else if (err == 1) {
        _DEBUG("%s\n", "packet was added ...");
    } else {
        _DEBUG("ERROR: %d SOMETHING IS WRONG WITH WINDOW\n", err);
    }
    return 1;
}

int cli_ack(int sockfd, char **wnd) {
    int err;
    /*todo: window stuff*/
    wnd++;
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
