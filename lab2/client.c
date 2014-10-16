#include "client.h"

int main(void) {
    char *path = "./client.in";
    char transferpath[BUFF_SIZE];
    FILE *file;
    char ip4_str[INET_ADDRSTRLEN];
    char buf[BUFF_SIZE + 1];
    /* config/xtcp vars */
    uint16_t windsize;
    int seed;
    float pktloss;
    int u; /* (!!in ms!!) mean of the expon dist func */

    /* connfd -- the main server connection socket */
    /* servfd -- the socket "accept" server socket */
    int connfd, servfd;
    uint16_t knownport; /* , serv_port; */

    /* main server connection address */
    struct sockaddr_in myaddr;
    /* zero the sockaddr_in */
    memset((void *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;

    /* read the config */
    file = fopen(path, "r");
    if(file == NULL) {
        perror("could not open server config file");
        exit(EXIT_FAILURE);
    }
    /* 1. fill in the server ip address */
    str_from_config(file, ip4_str, sizeof(ip4_str),
        "client.in:1: error getting IPv4 address");
    inet_pton(AF_INET, ip4_str, &(myaddr.sin_addr));
    /* 2. fill in the server port */
    knownport = int_from_config(file, "client.in:2: error getting port");
    myaddr.sin_port = htons(knownport);
    /* 3. fill in file to transfer */
    str_from_config(file, transferpath, sizeof(transferpath), 
        "client.in:3: error getting transfer file name");
    /* 4. fill in file to transfer */
    windsize = int_from_config(file, "client.in:4: error getting window size");
    /* 5. fill in seed */
    seed = int_from_config(file, "client.in:5: error getting seed");
    /* 6. fill in seed */
    pktloss = float_from_config(file, 
        "client.in:6: error getting packet loss percentage");
    /* 7. fill in seed */
    u = int_from_config(file, "client.in:7: error getting seed");

    _DEBUG("ipv4:%s port:%hu trans:%s wlen:%hu seed:%d pktloss:%5.4f u:%d\n",
        ip4_str, knownport, transferpath, windsize, seed, pktloss, u);

    /* get a socket to talk to the connection server port */ 
    connfd = socket(AF_INET, SOCK_DGRAM, 0);
    /* get a socket to talk to the server */ 
    servfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(connfd < 0 || servfd < 0){
        perror("client.socket()");
        exit(EXIT_FAILURE);
    }

    strncpy(buf, "SENDMESTUFF", sizeof(buf));
    /* flags could be MSG_DONTROUTE */ 
    sendto(connfd, buf, strlen(buf), 0, (struct sockaddr *)&myaddr, sizeof(myaddr));

    printf("Sent stuff to server\n");

    return EXIT_SUCCESS;
}
