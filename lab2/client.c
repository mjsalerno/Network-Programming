#include "client.h"

static uint32_t seq;
static uint32_t ack_seq; /* also used by wnd as next expected seq */
static uint16_t advwin;
static struct iface_info* ifaces;

extern double pkt_loss_thresh; /* packet loss percentage */
static double u; /* (!!in ms!!) mean of the exponential distribution func */

static struct window* w;
extern pthread_mutex_t w_mutex;

int main(void) {
    ssize_t err; /* for error checking */
    char *path = "client.in"; /* config path */
    char fname[BUFF_SIZE]; /* file to transfer */
    FILE *file; /* config file */
    char ip4_str[INET_ADDRSTRLEN];
    /* char buf[BUFF_SIZE + 1]; */
    /* config/xtcp vars */
    int seed;
    struct iface_info* iface_ptr;

    pthread_t consumer_tid;
    void *consumer_rtn;

    /* serv_fd -- the main server connection socket, reconnected later */
    int serv_fd;

    uint16_t knownport; /* , trans_port; */
    /* my_addr -- my (client) address */
    /* serv_addr -- main server connection address */
    /* bind_addr -- for getsockname() on client socket after bind */
    /* peer_addr -- for getpeername() on client socket after connect */
    struct sockaddr_in my_addr, serv_addr, bind_addr, peer_addr;

    /* struct xtcphdr hdr;*/ /* header for the init conn request */

    /* zero the sockaddr_in's */
    memset((void *)&my_addr, 0, sizeof(my_addr));
    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    my_addr.sin_family = AF_INET;
    /* my IP defaults to "arbitrary" when server non_local */
    /* my_addr.sin_addr.s_addr = htonl(INADDR_ANY); now using ify stuff */
    my_addr.sin_port = htons(0);
    serv_addr.sin_family = AF_INET;
    /* read the config */
    file = fopen(path, "r");
    if(file == NULL) {
        perror("could not open client config file");
        exit(EXIT_FAILURE);
    }
    /* 1. fill in the server ip address */
    str_from_config(file, ip4_str, sizeof(ip4_str),
        "client.in:1: error getting IPv4 address");
    err = inet_pton(AF_INET, ip4_str, &(serv_addr.sin_addr));
    if(err <= 0){
        fprintf(stderr, "inet_pton() invalid IPv4 address\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    /* 2. fill in the server port */
    knownport = (uint16_t) int_from_config(file, "client.in:2: error getting port");
    serv_addr.sin_port = htons(knownport);
    /* 3. fill in file to transfer */
    str_from_config(file, fname, sizeof(fname),
        "client.in:3: error getting transfer file name");
    /* 4. fill in file to transfer */
    advwin = (uint16_t) int_from_config(file, "client.in:4: error getting window size");

    /* 5. fill in seed */
    seed = int_from_config(file, "client.in:5: error getting seed");
    /* 6. fill in seed */
    pkt_loss_thresh = double_from_config(file,
        "client.in:6: error getting packet loss percentage");
    /* 7. fill in mean */
    u = double_from_config(file, "client.in:7: error getting mean");

    /* close the config file */
    fclose(file);
    /* set the seed for drand48() */
    srand48(seed);

    _DEBUG("config file args below:\nipv4:%s \nport:%hu \nfname:%s \n"
            "winsize:%hu \nseed:%d \np:%5.4f \nu:%5.4f\n\n",
        ip4_str, knownport, fname, advwin, seed, pkt_loss_thresh, u);

    /* do iffy info */
    ifaces = make_iface_list();
    print_iface_list(ifaces);
    inet_aton(ip4_str, &(my_addr.sin_addr));
    iface_ptr = get_matching_iface(ifaces, my_addr.sin_addr.s_addr);
    if(iface_ptr == NULL) {
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        my_addr.sin_addr.s_addr = iface_ptr->ip;
    }

    /* get a socket to talk to the server */
    serv_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(serv_fd < 0){
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    /* bind to my ip */
    err = bind(serv_fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if(err < 0){
        perror("bind()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }

    /* call on bind'ed socket getsockname(), i.e. print_sock_name */
    printf("bind()'ed to -- ");
    print_sock_name(serv_fd, &bind_addr);

    /* connect to server ip */
    if(0x100007F == my_addr.sin_addr.s_addr) {
        _DEBUG("%s\n", "I am connected to 127.0.0.1, so changing server ip");
        serv_addr.sin_addr.s_addr = 0x100007F;
    }

    err = connect(serv_fd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(err < 0){
        perror("connect()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }

    /* call on connect'ed socket getpeername(), i.e. print_sock_peer */
    printf("connect()'ed to -- ");
    print_sock_peer(serv_fd, &peer_addr);

    _DEBUG("%s\n", "creating wnd mutex...");
    err = init_wnd_mutex();
    if(err < 0) {
        fprintf(stderr, "ERROR: failed to make wnd mutex due to above error\n");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }

    err = handshakes(serv_fd, &serv_addr, fname);
    if(err != 0){
        close(serv_fd);
        exit(EXIT_FAILURE);
    }
    /* connected to file transfer port */

    /* pthread_create consumer thread */
    _DEBUG("%s\n", "creating pthread for consumer...");
    err = pthread_create(&consumer_tid, NULL, &consumer_main, NULL);
    if(0 > err){
        errno = (int)err;
        perror("ERROR: consumer pthread_create()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }


    printf("waiting for the file...\n");

    for(EVER) {

        _DEBUG("%s\n", "calling clirecv() to recv file data");
        err = clirecv(serv_fd, w);
        _DEBUG("clirecv() returned: %d\n", (int)err);
        if (err <= -2) {
            _DEBUG("%s\n", "client exit(EXIT_FAILURE)");
            exit(EXIT_FAILURE);
        }
        else if(err == -1){
            _ERROR("%s\n", "Got a RST, ending client...");
            close(serv_fd);
            exit(EXIT_SUCCESS);
        }
        else if(err == 0) {
            _DEBUG("%s\n", "Done recv'ing the file...");
            break;
        }
        /* got actual data, loop back around for more */
        _DEBUG("%s\n", "clirecv() got actual data, loop back around for more");

    }

    /* wake up when the window is empty */
    _DEBUG("%s\n", "joining on consumer thread.....");
    err = pthread_join(consumer_tid, &consumer_rtn);
    if(err > 0) {
        errno = (int)err;
        perror("ERROR: pthread_join()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }

    /* todo: use consumer return ?? */

    _DEBUG("%s\n", "success! closing serv_fd then exiting...");
    close(serv_fd);
    exit(EXIT_SUCCESS);
}

/**
* -1 if timedout, 0 if connected.
*/
int handshakes(int serv_fd, struct sockaddr_in *serv_addr, char *fname) {
    char pktbuf[MAX_PKT_SIZE];
    struct xtcphdr *hdr; /* just to cast pktbuf to xtcphdr type */
    struct xtcphdr *sendpkt;
    /* select vars */
    fd_set rset;
    int maxfpd1 = 0;
    ssize_t n, err;
    struct timeval timer;
    int attemp = 0;

    /* init seq/ack_seq */
    seq = (uint32_t)lrand48();
    ack_seq = 0;

    sendpkt = alloc_pkt(seq, ack_seq, SYN, advwin, fname, strlen(fname));

    sendagain: /* jump here if server doesn't respond */
    if(attemp >= 3) {
        fprintf(stderr, "Too many retries......Exiting....Goodbye\n");
        free(sendpkt);
        return -1;
    }
    attemp++;
    printf("try send hs1\n");
    clisend(serv_fd, sendpkt, strlen(fname));
    /* don't increment seq until we know this sent */

    _DEBUG("%s\n", "waiting for hs2...");

    /* select() for(ever) for SYN */
    /* todo: timeout for dropped packets! */
    for(EVER) {
        FD_ZERO(&rset);
        FD_SET(serv_fd, &rset);
        timer.tv_sec = TIME_OUT * attemp;
        timer.tv_usec = 0;
        maxfpd1 = serv_fd + 1;

        printf("will wait %lusecs to recv hs2\n", timer.tv_sec);
        err = select(maxfpd1, &rset, NULL, NULL, &timer);
        if(err < 0) {
            /* EINTR can't occur yet */
            perror("handshakes().select()");
            exit(EXIT_FAILURE);
        }
        /* check if the serv_fd is set */
        if(FD_ISSET(serv_fd, &rset)) {
            _DEBUG("%s\n", "recv()'ing something");
            n = recv(serv_fd, pktbuf, sizeof(pktbuf), 0);
            if (n < 0) {
                perror("handshakes().recv()");
                exit(EXIT_FAILURE);
            }
            /* we recv()'ed so test if we should keep it */
            _DEBUG("%s\n", "recv'd a packet, should we drop it?");
            if(DROP_PKT()) {
                /* we dropped it, note it, and continue */
                _NOTE("%s", "DROPPED RECV'ing PKT: ");
                ntohpkt((struct xtcphdr*)pktbuf);
                print_hdr((struct xtcphdr *)pktbuf);
                continue;
            }else {
                /* we recv()'ed something so break from select */
                _DEBUG("%s\n", "we kept it.");
                break;
            }

        }
        else { /* so the select timed out */
            _DEBUG("%s\n", "timeout() while waiting for hs2, resending hs1");
            /* hs1 was lost or hs2 was lost so */
            goto sendagain;
        }
    }
    free(sendpkt);
    /* the server responded so now inc seq */
    ++seq;
    /* validate the something was a SYN-ACK */
    hdr = (struct xtcphdr*)pktbuf;
    ntohpkt(hdr);
    err = validate_hs2(hdr, (int)n);
    if(err < 0){
        return -1;
    }


    print_hdr(hdr);
    ack_seq = hdr->seq + 1; /* we expect their seq + 1 */

    /* copy the passed port into the serv_addr */
    memcpy(&serv_addr->sin_port, pktbuf + DATA_OFFSET, sizeof(serv_addr->sin_port));
    /* re connect() with new port */
    err = connect(serv_fd, (const struct sockaddr*)serv_addr, sizeof(struct sockaddr));
    if (err < 0) {
        perror("re connect()");
        exit(EXIT_FAILURE);
    }
    printf("re-connect()'ed to -- ");
    print_sock_peer(serv_fd, serv_addr);

    /* move on to third handshake */
    /* init the window and the wnd_base_seq */
    /* consumer not alive, don't lock */
    w = init_window(advwin, 0, 0, ack_seq, ack_seq);

    sendpkt = alloc_pkt(seq, ack_seq, ACK, advwin, NULL, 0);
    printf("try send hs3: \n");

    /* todo: put hs2 into window */
    clisend(serv_fd, sendpkt, 0);

    free(sendpkt);
    return 0;

}

/* hdr must be in host byte order */
int validate_hs2(struct xtcphdr* hdr, int len){
    if(hdr->flags != (SYN|ACK)){
        fprintf(stderr, "hs2 not a SYN-ACK, flags: %d\n", hdr->flags);
        /* todo: RST/ free */
        return -1;
    }
    /* now validate the SEQ and ACK nums */
    if(hdr->ack_seq != seq){
        fprintf(stderr, "ERROR: hs2 unexpected ack_seq: %d, expected: %d\n", hdr->ack_seq, seq+1);
        /* todo: RST/ free */
        return -1;
    }
    if(len != DATA_OFFSET + 2){
        fprintf(stderr, "hs2 not %d bytes, size: %d\n", DATA_OFFSET + 2, len);
        /* todo: RST/ free */
        return -1;
    }
    return 0;
}

/**
* TODO: return code for we are done (we have sent an ACK for the FIN pkt)
* clirecv -- for the client/receiver
* Returns: >0 bytes recv'd
*           0 on FIN recv'd
*          -1 on RST recv'd
* DESC:
* Blocks until a packet is recv'ed from sockfd. If it's a FIN, note the seq num and recv
* until you have ACKed that you expect the FIN's seq_num.
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
    int err;
    fd_set rset;
    char *pkt = malloc(MAX_PKT_SIZE + 1); /* +1 for consumer and printf */
    if(pkt == NULL){
        perror("clirecv().malloc()");
        exit(EXIT_FAILURE);
    }
    for(EVER) {
        /*continue_with_select: */
        /* todo: remove this print? */
        _DEBUG("%s\n", "select()'ing on server socket for pkts");

        get_lock(&w_mutex);
        print_window(w);
        unget_lock(&w_mutex);

        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        /* todo: add 12 * 3 or 40 second timeout on select() */

        err = select(sockfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0){
            if(err == EINTR) {
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
                exit(EXIT_FAILURE);
            }else if(bytes < 12){
                _DEBUG("clirecv().recv() not long enough to be pkt: %dbytes\n", (int)bytes);
                continue;
            }
            ntohpkt((struct xtcphdr *)pkt); /* host order please */
            pkt[bytes] = 0; /* NULL terminate the ASCII text */

            /* if it's a FIN or RST don't try to drop it, we're closing dirty! */
            if((((struct xtcphdr*)pkt)->flags & FIN) == FIN) {
                /* FIXME: buffer FIN */
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
            if(DROP_PKT()) {
                /* drop the pkt */
                _NOTE("%s", "DROPPED RECV'ing PKT: ");
                print_hdr((struct xtcphdr *) pkt);
                continue;
            } else {
                /**
                * keep the pkt:
                * Pretend like the code after the "break;" is in here.
                * However, because it's not in here it will use a goto
                * instead of a continue later.
                */
                _DEBUG("keeping pkt with seq: %"PRIu32", add and send from window.\n", ((struct xtcphdr*)pkt)->seq);
                err = cli_add_send(sockfd, (struct xtcphdr*)pkt, (int)bytes, w);
                /* todo: check return codes */
                /* fixme: don't break here? */
                return 1;
            }
        } else{
            /* todo: add timeout */
        }
        /* end of select for(EVER) */
    }
    /* fixme: what is here? */
    return -1;
}

/**
* The client only directly calls this function twice, for the SYN and first ACK.
* It provides packet loss to the client's sends.
*
* pkt is in HOST ORDER please!
*/
void clisend(int sockfd, struct xtcphdr *pkt, size_t datalen) {
    _DEBUG("%s\n", "printing hdr to send");
    print_hdr((struct xtcphdr*)pkt);
    clisend_lossy(sockfd, pkt, datalen);
}

/**
* init the wnd mutex
*/
int init_wnd_mutex(void){
    int err = 0;
    pthread_mutexattr_t attr;

    err = pthread_mutexattr_init(&attr);
    if(err > 0){
        errno = err;
        perror("ERROR pthread_mutexattr_init()");
        return -1;
    }
    err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if(err > 0){
        errno = err;
        perror("ERROR pthread_mutexattr_settype()");
        return -1;
    }

    err = pthread_mutex_init(&w_mutex, &attr);
    if(err > 0){
        errno = err;
        perror("ERROR pthread_mutex_init()");
        return -1;
    }

    err = pthread_mutexattr_destroy(&attr);
    if(err > 0){
        errno = err;
        perror("ERROR pthread_mutexattr_destroy()");
        return -1;
    }

    return 0;
}

void *consumer_main(void *null) {
    double msecs_d;
    unsigned int usecs;
    int fin_found = 5;
    int err;
    srand48(time(NULL));
    /* u is in milliseconds ms! not us, not ns*/
    _NOTE("%s","CONSUMER: consumer created\n");

    /* fixme: break when fin found */
    while(fin_found) {
        msecs_d = -1 * u * log(drand48()); /* -1 × u × ln( drand48( ) ) */

        usecs = 1000 * (unsigned int) round(msecs_d);


        usleep(usecs);
        /*get_lock(&w_mutex);

        _NOTE("CONSUMER: woke up after sleeping %fms, has the lock\n", msecs_d);*/

        /* fixme: read from wnd */
        /*consumer_read();*/

        /*unget_lock(&w_mutex);*/
    }

    err = pthread_mutex_destroy(&w_mutex);
    if(err > 0){
        errno = err;
        perror("CONSUMER: ERROR pthread_mutex_destroy()");
        exit(EXIT_FAILURE);
    }
    /* todo: change */
    return null;
}

/**
* fixme: after reading fin die!
* fixme: count total bytes/pkts read and return if read FIN
*/
int consumer_read() {
    struct win_node* at;
    int count = 0;
    at = w->base;

    for(; at->datalen > 0; at = at->next, ++count) {
        if ((at->pkt->flags & FIN) == FIN) {
            _NOTE("%s\n", "consumer has reached FIN");
            break;
        }
        printf("%s", (char*)((at->pkt) + DATA_OFFSET));
        at->datalen = -1;
    }

    w->base = at;

    return count;
}
