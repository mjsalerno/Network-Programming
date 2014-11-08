#include <time.h>
#include "server.h"

int main(void) {
    int sock;
    int port;
    time_t ticks;
    struct sockaddr_un name;
    char buff[BUFF_SIZE];
    char ip[INET_ADDRSTRLEN];


    /* Create socket from which to read. */
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("opening datagram socket");
        exit(1);
    }

    /* Create name. */
    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, NAME);


    /* Bind the UNIX domain address to the created socket */
    if (bind(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_un))) {
        perror("binding name to datagram socket");
        exit(1);
    }
    printf("socket -->%s\n", NAME);


    /* Read from the socket */
    if (read(sock, buff, 1024) < 0)
        perror("receiving datagram packet");
    printf("-->%s\n", buff);

    msg_recv(sock, buff, BUFF_SIZE, ip, &port);

    ticks = time(NULL);
    snprintf(buff, sizeof(buff), "%.24s\n", ctime(&ticks));

    msg_send(sock, ip, port, buff, 3, 0);

    close(sock);
    unlink(NAME);

    return 1;
}
