#include <assert.h>
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

    /* conn_fd -- the main server connection socket */
    /* trans_fd -- the socket "accept" server socket */
    int conn_fd, trans_fd;
    /* select vars */
    fd_set rset;
    int maxfpd1 = 0;

    uint16_t knownport; /* , trans_port; */
    /* main server connection address */
    struct sockaddr_in conn_addr;
    /* file transfer address */
    struct sockaddr_in trans_addr;

    /* for pedantic */
    /* if(windsize || seed || u || p){} */

    /* zero the sockaddr_in's */
    memset((void *)&conn_addr, 0, sizeof(conn_addr));
    memset((void *)&trans_addr, 0, sizeof(trans_addr));
    conn_addr.sin_family = AF_INET;
    trans_addr.sin_family = AF_INET;

    /* read the config */
    file = fopen(path, "r");
    if(file == NULL) {
        perror("could not open client config file");
        exit(EXIT_FAILURE);
    }
    /* 1. fill in the server ip address */
    str_from_config(file, ip4_str, sizeof(ip4_str),
        "client.in:1: error getting IPv4 address");
    err = inet_pton(AF_INET, ip4_str, &(conn_addr.sin_addr));
    if(err <= 0){
        fprintf(stderr, "client.inet_pton() invalid IPv4 address\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    err = inet_pton(AF_INET, ip4_str, &(trans_addr.sin_addr));
    if(err <= 0){
        fprintf(stderr, "client.inet_pton() invalid IPv4 address\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    /* 2. fill in the server port */
    knownport = (uint16_t) int_from_config(file, "client.in:2: error getting port");
    conn_addr.sin_port = htons(knownport);
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

    /* get a socket to talk to the connection server port */ 
    conn_fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* get a socket to talk to the server */ 
    trans_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(conn_fd < 0 || trans_fd < 0){
        perror("client.socket()");
        exit(EXIT_FAILURE);
    }

    for(;;) {
        FD_ZERO(&rset);
        FD_ISSET(conn_fd, &rset);
        maxfpd1 = conn_fd + 1;
        assert(maxfpd1);

        strncpy(buf, "SEND ACK 1 SEQ 0", sizeof(buf));
        /* flags could be MSG_DONTROUTE */
        /* timeout  on oldest packet */

        /* simulate packet loss on sends*/
        if(drand48() > p) {
            err = sendto(conn_fd, buf, strlen(buf), 0,
                    (struct sockaddr *) &conn_addr, sizeof(conn_addr));
            if (err < 0) {
                perror("client.sendto()");
                close(conn_fd);
                exit(EXIT_FAILURE);
            }
            printf("Sent connection req to server\n");
        }

    }

    close(conn_fd);

    return EXIT_SUCCESS;
}
