#include "server.h"

int main(void) {
    int sock;
    ssize_t err, n;
    time_t ticks;
    int cliport;
    struct hostent *vm;
    struct sockaddr_un name;
    /*struct sockaddr_un cli_addr;*/

    struct in_addr vm_ip;
    char buff[BUFF_SIZE];
    char cli_ip_buff[INET_ADDRSTRLEN];
    char hostname[BUFF_SIZE];

    err = gethostname(hostname, sizeof(hostname));
    if(err < 0) {
        perror("ERROR: gethostname()");
        exit(EXIT_FAILURE);
    }
    printf("host: %s\n", hostname);

    /* Create socket from which to read. */
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("opening datagram socket");
        exit(EXIT_FAILURE);
    }

    /* Create name. */
    bzero(&name, sizeof(struct sockaddr_un));
    name.sun_family = AF_UNIX;
    strcpy(name.sun_path, TIME_SRV_PATH);

    unlink(TIME_SRV_PATH);

    /* Bind the UNIX domain address to the created socket */
    err = bind(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_un));
    if(err < 0) {
        perror("binding name to datagram socket");
        close(sock);
        unlink(TIME_SRV_PATH);
        exit(EXIT_FAILURE);
    }
    printf("socket --> %s\n", TIME_SRV_PATH);

    for(EVER) {
        _DEBUG("%s", "waiting for clients...\n");
        /*todo: fix ip*/
        n = msg_recv(sock, buff, BUFF_SIZE, cli_ip_buff, &cliport);
        if(n < 0) {
            perror("ERROR: msg_recv()");
            close(sock);
            unlink(TIME_SRV_PATH);
            exit(EXIT_FAILURE);
        }
        buff[n] = '\0';
        _DEBUG("client! len: %d, mesg: %s\n", (int)n, buff);
        inet_aton(cli_ip_buff, &vm_ip);
        vm = gethostbyaddr(&vm_ip, sizeof(vm_ip), AF_INET);
        if (vm == NULL) {
            herror("server.gethostbyaddr()");
            close(sock);
            unlink(TIME_SRV_PATH);
            exit(EXIT_FAILURE);
        }
        printf("server at node: %s responding to request from: %s\n", hostname, vm->h_name);

        ticks = time(NULL);
        n = snprintf(buff, sizeof(buff), "%.24s\n", ctime(&ticks));

        err = msg_send(sock, cli_ip_buff, cliport, buff, (size_t)n, 0);
        if(err < 0) {
            perror("ERROR: msg_send()");
            close(sock);
            unlink(TIME_SRV_PATH);
            exit(EXIT_FAILURE);
        }
    }

    close(sock);
    unlink(TIME_SRV_PATH);
    exit(EXIT_SUCCESS);
}
