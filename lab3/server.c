#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "server.h"

int main(void) {
    int sock;
    int port;
    time_t ticks;
    struct hostent *vm;
    struct sockaddr_un name;
    struct in_addr vm_ip;
    char buff[BUFF_SIZE];
    char ip[INET_ADDRSTRLEN];

    gethostname(buff, BUFF_SIZE);
    printf("host: %s\n", buff);

    /* Create socket from which to read. */
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("opening datagram socket");
        exit(1);
    }

    /* Create name. */
    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, KNOWN_PATH);

    unlink(KNOWN_PATH);

    /* Bind the UNIX domain address to the created socket */
    if (bind(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_un))) {
        perror("binding name to datagram socket");
        exit(1);
    }
    printf("socket --> %s\n", KNOWN_PATH);

    for(EVER) {

        msg_recv(sock, buff, BUFF_SIZE, ip, &port);

        inet_aton(ip, &vm_ip);
        vm = gethostbyaddr(&vm_ip, sizeof(vm_ip), AF_INET);
        if (vm == NULL) {
            herror("server.gethostbyaddr()");
            exit(EXIT_FAILURE);
        }
        printf("server at node: %s", vm->h_name);

        inet_aton("127.0.0.1", &vm_ip);
        vm = gethostbyaddr(&vm_ip, sizeof(vm_ip), AF_INET);
        if (vm == NULL) {
            herror("server.gethostbyaddr()");
            exit(EXIT_FAILURE);
        }
        printf("  responding to request from: %s\n", vm->h_name);

        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\n", ctime(&ticks));

        msg_send(sock, ip, port, buff, 3, 0);
    }

    close(sock);
    unlink(KNOWN_PATH);

    return 1;
}
