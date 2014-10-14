#include "client.h"

int main(void) {
    char buf[BUFF_SIZE + 1];
    /* connfd -- the main server connection socket */
    /* servfd -- the socket "accept" server socket */
    int connfd, servfd;
    //unsigned short conn_port, serv_port;

    /* main server connection address */
    struct sockaddr_in sa;
    /* zero the sockaddr_in */
    memset((void *)&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    /* fill in the well-known port num */
    /* sa.sin_port = htons(SERV_PORT);*/

    /*I have changed it to this because the server reads from a config file
      we should make the client read from a config too.*/
    sa.sin_port = htons(6758);
    /* fill in the server ip address */
    //inet_pton(AF_INET, serv_ip, &(sa.sin_addr));

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
    sendto(connfd, buf, strlen(buf), 0, (struct sockaddr *)&sa, sizeof(sa));

    printf("Sent stuff to server\n");

    return EXIT_SUCCESS;
}
