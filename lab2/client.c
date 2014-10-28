#include "client.h"

extern uint32_t seq;
extern uint32_t ack_seq; /* also used by wnd as next expected seq */
extern uint16_t advwin;

static char** wnd; /* will point to malloc()'d array from init_wnd()*/

extern double pkt_loss_thresh; /* packet loss percentage */

static double u; /* (!!in ms!!) mean of the exponential distribution func */

int main(void) {
    ssize_t err; /* for error checking */
    char *path = "client.in"; /* config path */
    char fname[BUFF_SIZE]; /* file to transfer */
    FILE *file; /* config file */
    char ip4_str[INET_ADDRSTRLEN];
    /* char buf[BUFF_SIZE + 1]; */
    /* config/xtcp vars */
    uint16_t orig_win_size;
    int seed;

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
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
    orig_win_size = advwin;
    orig_win_size++;
    orig_win_size--;

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
        ip4_str, knownport, fname, orig_win_size, seed, pkt_loss_thresh, u);

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
    err = connect(serv_fd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(err < 0){
        perror("connect()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }

    /* call on connect'ed socket getpeername(), i.e. print_sock_peer */
    printf("connect()'ed to -- ");
    print_sock_peer(serv_fd, &peer_addr);

    err = handshakes(serv_fd, &serv_addr, fname);
    if(err != 0){
        /* todo: clean up, close?, free?*/
        close(serv_fd);
        exit(EXIT_FAILURE);
    }
    /* connected to file transfer port */

    /* pthread_create consumer thread */
    /* todo: atttr not NULL */
    _DEBUG("%s\n", "creating pthread for consumer...");
    err = pthread_create(&consumer_tid, NULL, &consumer_main, (void *)wnd);
    if(0 > err){
        errno = (int)err;
        perror("ERROR: consumer pthread_create()");
        close(serv_fd);
        exit(EXIT_FAILURE);
    }


    _DEBUG("%s\n", "waiting for the file ...");
    for(EVER) {
        print_wnd((const char**)wnd);
        _DEBUG("%s\n", "calling clirecv()");
        err = clirecv(serv_fd, wnd);
        _DEBUG("clirecv() returned: %d\n", (int)err);
        if (err <= -2) {
            _DEBUG("%s\n", "client exit(EXIT_FAILURE)");
            exit(EXIT_FAILURE);
        }
        else if(err == -1){
            _DEBUG("%s\n", "Got RST, ending client...");
            break;
        }
        else if(err == 0) {
            _DEBUG("%s\n", "Done getting file...");
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
    /* todo: use consumer_rtn ??*/

    _DEBUG("%s\n", "success! closing serv_fd then exiting...");
    close(serv_fd);
    return EXIT_SUCCESS;
}


int handshakes(int serv_fd, struct sockaddr_in *serv_addr, char *fname) {
    char pktbuf[MAX_PKT_SIZE];
    struct xtcphdr *hdr; /* just to cast pktbuf to xtcphdr type */
    /* select vars */
    fd_set rset;
    int maxfpd1 = 0;
    ssize_t n, err;
    struct timeval timer;

    /* init seq/ack_seq */
    seq = (uint32_t)lrand48();
    ack_seq = 0;

    /* todo: timeout  on oldest packet */

    sendagain: /* jump here if server doesn't respond */
    printf("try send hs1\n");
    err = clisend(serv_fd, SYN, fname, strlen(fname));
    if(err < 0){
        _DEBUG("handshakes.clisend() returned: %d\n", (int)err);
        return -1;
    }
    /* don't increment seq until we know this sent */

    _DEBUG("%s\n", "waiting for hs2...");

    /* select() for(ever) for SYN */
    /* todo: timeout for dropped packets! */
    for(EVER) {
        FD_ZERO(&rset);
        FD_SET(serv_fd, &rset);
        timer.tv_sec = TIME_OUT;
        timer.tv_usec = 0;
        maxfpd1 = serv_fd + 1;

        err = select(maxfpd1, &rset, NULL, NULL, &timer);
        if(err < 0) {
            /* EINTR can't occur yet */
            perror("hs1.select()");
            return -1;
        }
        /* check if the serv_fd is set */
        if(FD_ISSET(serv_fd, &rset)) {
            _DEBUG("%s\n", "recv()'ing something");
            n = recv(serv_fd, pktbuf, sizeof(pktbuf), 0);
            if (n < 0) {
                perror("recv()");
                return -1;
            }
            /* we recv()'ed something so break from select */
            break;
        }
        else { /* so the select timed out */
            _DEBUG("%s\n", "timeout() while waiting for hs2, resending hs1");
            /* hs1 was lost or hs2 was lost so */
            goto sendagain;
        }
    }
    /* the server responded so now inc seq */
    ++seq;
    /* validate the something was a SYN-ACK */
    hdr = (struct xtcphdr*)pktbuf;
    ntohpkt(hdr);
    err = validate_hs2(hdr, (int)n);
    if(err < 0){
        return -1;
    }

    printf("recv'd hs2: ");
    print_hdr(hdr);
    ack_seq = hdr->seq + 1; /* we expect their seq + 1 */

    /* pick the smallest advwin */
    advwin = ((hdr)->advwin < advwin) ? (hdr)->advwin : advwin;
    _DEBUG("new advwin: %d\n", advwin);

    /* copy the passed port into the serv_addr */
    memcpy(&serv_addr->sin_port, pktbuf + DATA_OFFSET, sizeof(serv_addr->sin_port));
    /* re connect() with new port */
    err = connect(serv_fd, (const struct sockaddr*)serv_addr, sizeof(struct sockaddr));
    if (err < 0) {
        perror("re connect()");
        return -1;
    }
    printf("re-connect()'ed to -- ");
    print_sock_peer(serv_fd, serv_addr);

    /* move on to third handshake */
    /* init the window and the wnd_base_seq */
    wnd = init_wnd(ack_seq);
    if(wnd == NULL){
        return -1;
    }

    /* todo: back by ARQ */
    printf("try send hs3: \n");
    /* don't use cli_ack() here */
    err = clisend(serv_fd, ACK, NULL, 0);
    if(err < 0){
        return -1;
    }

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

void *consumer_main(void *wnd) {
    double msecs_d;
    unsigned int usecs;
    int fin_found = 5;
    srand48(time(NULL));
    /* u is in milliseconds ms! not us, not ns*/

    while(fin_found--) {
        msecs_d = -1 * u * log(drand48()); /* -1 × u × ln( drand48( ) ) */

        usecs = 1000 * (unsigned int) round(msecs_d);
        _DEBUG("CONSUMER: sleeping for:  %fms\n", msecs_d);
        _DEBUG("CONSUMER: we rounded to: %uus\n", usecs);

        usleep(usecs);

        /* todo: read from wnd */
    }

    /* todo: change */
    return wnd;
}

