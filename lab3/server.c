#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "server.h"

int main(void) {
    int sock;
    /*int port;*/
    ssize_t err;
    time_t ticks;
    struct hostent *vm;
    struct sockaddr_un name;
    struct sockaddr_in cli_addr;
    socklen_t len;

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
    bzero(&name, sizeof(struct sockaddr_un));
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

        err = recvfrom(sock, buff, BUFF_SIZE, 0, (struct sockaddr*)&cli_addr, &len);
        if(err < 0) {
            printf("there was an error from recv");
        }

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

        printf("here\n");

        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\n", ctime(&ticks));

        printf("here\n");

        err = sendto(sock, buff, BUFF_SIZE, 0, (struct sockaddr*) &cli_addr, len);
        if(err < 0) {
            printf("there was an error sending\n");
            exit(EXIT_FAILURE);
        }
        printf("here\n");
    }

    close(sock);
    unlink(KNOWN_PATH);

    return 1;
}
