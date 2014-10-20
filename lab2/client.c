#include "client.h"

int main(void) {
    ssize_t err; /* for error checking */
    char *path = "./client.in"; /* config path */
    char transferpath[BUFF_SIZE]; /* file to transfer */
    FILE *file; /* config file */
    char ip4_str[INET_ADDRSTRLEN];
    char buf[BUFF_SIZE + 1];
    /* config/xtcp vars */
    uint16_t windsize;
    int seed;
    double p; /* packet loss percentage */
    double u; /* (!!in ms!!) mean of the exponential dist func */

    /* serv_fd -- the main server connection socket, reconnected later */
    int serv_fd;
    /* select vars */
    fd_set rset;
    int maxfpd1 = 0;

    uint16_t knownport; /* , trans_port; */
    /* my_addr -- my (client) address */
    /* serv_addr -- main server connection address */
    /* bind_addr -- for getsockname() on client socket after bind */
    /* peer_addr -- for getpeername() on client socket after connect */
    struct sockaddr_in my_addr, serv_addr, bind_addr, peer_addr;

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
        fprintf(stderr, "client.inet_pton() invalid IPv4 address\n");
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
        perror("client.bind()");
        exit(EXIT_FAILURE);
    }
    /* bind to my ip */
    err = bind(serv_fd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if(err < 0){
        perror("client.bind()");
        exit(EXIT_FAILURE);
    }

    /* call on bind'ed socket getsockname(), i.e. printsockname */
    printf("client bind()'ed to -- ");
    print_sock_name(serv_fd, &bind_addr);

    /* connect to server ip */
    err = connect(serv_fd, (const struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(err < 0){
        perror("client.connect()");
        exit(EXIT_FAILURE);
    }
    /* getpeername(), print */
    printf("client connect()'ed to -- ");
    print_sock_peer(serv_fd, &peer_addr);

    strncpy(buf, "SEND ACK 1 SEQ 0", sizeof(buf));
    /* timeout  on oldest packet */

    /* simulate packet loss on sends*/
    if(drand48() > p) {
        err = sendto(serv_fd, buf, strlen(buf), 0,
                (struct sockaddr *) &serv_addr, sizeof(serv_addr));
        if (err < 0) {
            perror("client.sendto()");
            close(serv_fd);
            exit(EXIT_FAILURE);
        }
        printf("Sent connection req to server\n");
    }

    /* select() for(ever) for SYN */
    for(;;) {
        FD_ZERO(&rset);
        FD_SET(serv_fd, &rset);
        maxfpd1 = serv_fd + 1;

        /* todo: liveliness timer? */
        select(maxfpd1, &rset, NULL, NULL, NULL);

    }

    close(serv_fd);

    return EXIT_SUCCESS;
}
