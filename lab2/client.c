#include "client.h"

int main(void) {
    /*stuff mike added*/
    /*socklen_t len;
    ssize_t n;*/
    /*stuff mike added*/

    ssize_t err; /* for error checking */
    char *path = "/home/scott-harvey/git/cse533/lab2/client.in"; /* config path */
    char transferpath[BUFF_SIZE]; /* file to transfer */
    FILE *file; /* config file */
    char ip4_str[INET_ADDRSTRLEN];
    /* char buf[BUFF_SIZE + 1]; */
    /* config/xtcp vars */
    uint16_t windsize;
    int seed;
    double p; /* packet loss percentage */
    double u; /* (!!in ms!!) mean of the exponential distribution func */

    /* serv_fd -- the main server connection socket, reconnected later */
    int serv_fd;

    uint16_t knownport; /* , trans_port; */
    /* my_addr -- my (client) address */
    /* serv_addr -- main server connection address */
    /* bind_addr -- for getsockname() on client socket after bind */
    /* peer_addr -- for getpeername() on client socket after connect */
    struct sockaddr_in my_addr, serv_addr, bind_addr, peer_addr;

    /* struct xtcphdr hdr;*/ /* header for the init conn request */
    /* for pedantic */
    /* if(windsize || seed || u || p){} */

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
    str_from_config(file, transferpath, sizeof(transferpath),
        "client.in:3: error getting transfer file name");
    /* 4. fill in file to transfer */
    windsize = (uint16_t) int_from_config(file, "client.in:4: error getting window size");
    /* 5. fill in seed */
    seed = int_from_config(file, "client.in:5: error getting seed");
    /* 6. fill in seed */
    p = double_from_config(file,
        "client.in:6: error getting packet loss percentage");
    /* 7. fill in seed */
    u = double_from_config(file, "client.in:7: error getting seed");

    /* close the config file */
    fclose(file);
    /* set the seed for drand48() */
    srand48(seed);

    _DEBUG("config file args below:\nipv4:%s \nport:%hu \ntrans:%s \n"
            "windsize:%hu \nseed:%d \np:%5.4f \nu:%5.4f\n\n",
        ip4_str, knownport, transferpath, windsize, seed, p, u);

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

    err = handshakes(serv_fd, &serv_addr, p, transferpath, windsize);
    if(err != 0){
        /* todo: clean up, close?, free?*/
        close(serv_fd);
        exit(EXIT_FAILURE);
    }
    /* connected to file transfer port */
    /* todo: pthread_create consumer thread */




    close(serv_fd);

    return EXIT_SUCCESS;
}


int handshakes(int serv_fd, struct sockaddr_in *serv_addr, double p, char *transferpath, uint16_t windsize) {
    char pktbuf[MAX_PKT_SIZE];
    struct xtcphdr *hdr; /* just to cast pktbuf to xtcphdr type */
    void *packet;
    size_t packetlen;
    uint32_t seq = (uint32_t)lrand48(), ack_seq = 0;
    in_port_t newport;
    /* select vars */
    fd_set rset;
    int maxfpd1 = 0;
    double ran;
    ssize_t n, err;

    packetlen = DATAOFFSET + strlen(transferpath);
    packet = malloc(packetlen);
    /* make the packet */
    make_pkt(packet, ++seq, ack_seq, SYN, windsize, transferpath, strlen(transferpath));
    /* convert to network order */
    htonpkt((struct xtcphdr*)packet);

    /* todo: print_xtxphdr(&hdr); */
    /* todo: timeout  on oldest packet */

    /* simulate packet loss on sends */
    ran = drand48();
    _DEBUG("drand48() = %f\n", ran);
    _DEBUG("p = %f\n", p);
    if(ran > p) {
        err = send(serv_fd, packet, packetlen, 0);
        if (err < 0) {
            perror("sendto()");
            free(packet);
            return -1;
        }
        printf("sent 1st handshake (SYN)\n");
        ntohpkt((struct xtcphdr*)packet);
        print_xtxphdr((struct xtcphdr*)packet);
    }
    else{
        printf("dropped 1st handshake (SYN)\n");
    }
    _DEBUG("%s\n", "waiting for 2nd handshake...");

    /* select() for(ever) for SYN */
    /* todo: timeout for dropped packets! */
    for(;;) {
        FD_ZERO(&rset);
        FD_SET(serv_fd, &rset);
        maxfpd1 = serv_fd + 1;

        /* todo: liveliness timer? RTO/RTT timer */
        select(maxfpd1, &rset, NULL, NULL, NULL);
        /* check if the serv_fd is set */
        if(FD_ISSET(serv_fd, &rset)) {
            _DEBUG("%s\n", "recv()'ing something");
            n = recv(serv_fd, pktbuf, sizeof(pktbuf), 0);
            if (n < 0) {
                perror("recv()");
                free(packet);
                return -1;
            }
            /* we recv()'ed something so break from select */
            break;
        }
    }
    /* validate the something was a SYN-ACK */
    hdr = (struct xtcphdr*)pktbuf;
    ntohpkt(hdr);
    if(hdr->flags != (SYN|ACK)){
        fprintf(stderr, "2nd handshake not a SYN-ACK, flags: %d\n", hdr->flags);
        /* todo: RST/ free */
        free(packet);
        return -1;
    }
    /* now validate the SEQ and ACK nums */
    if(hdr->ack_seq != seq+1){
        fprintf(stderr, "2nd handshake ack_seq num wrong, ack_seq: %d\n", hdr->ack_seq);
        /* todo: RST/ free */
        free(packet);
        return -1;
    }
    if(n != DATAOFFSET + 2){
        fprintf(stderr, "2nd handshake not %d bytes, size: %d\n", DATAOFFSET + 2, (int)n);
        /* todo: RST/ free */
        free(packet);
        return -1;
    }
    printf("recv'd 2nd handshake\n");
    print_xtxphdr(hdr);
    ack_seq = hdr->seq;

    /* copy the passed port into a short to prevent SIGBUS */
    memcpy(&newport, pktbuf, sizeof(newport));
    serv_addr->sin_port = newport; /* set the new port */
    /* re connect() with new port */
    err = connect(serv_fd, (const struct sockaddr*)serv_addr, sizeof(struct sockaddr));
    if (err < 0) {
        perror("re connect()");
        return -1;
    }
    printf("re-connect()'ed to -- ");
    print_sock_peer(serv_fd, serv_addr);
    /* move on to third handshake */

    /* third handshake */
    make_pkt(packet, ++seq, ++ack_seq, ACK, windsize, NULL, 0);
    htonpkt((struct xtcphdr*)packet);

    /* todo: print_xtxphdr(&hdr); */
    /* todo: timeout  on oldest packet */

    /* simulate packet loss on sends */
    if(drand48() > p) {
        err = send(serv_fd, packet, DATAOFFSET, 0);
        if (err < 0) {
            perror("sendto()");
            free(packet);
            return -1;
        }
        printf("sent 3rd handshake (ACK)\n");
    }
    else{
        printf("dropped 3rd handshake (ACK)\n");
    }
    free(packet);
    return 0;

}
