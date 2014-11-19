#include "server.h"

static int sock;

void handle_sigint(int sign) {
    /**
    * From signal(7):
    *
    * POSIX.1-2004 (also known as POSIX.1-2001 Technical Corrigendum 2) requires an  implementation
    * to guarantee that the following functions can be safely called inside a signal handler:
    * ... _Exit() ... close() ... unlink() ...
    */
    sign++; /* for -Wall -Wextra -Werror */
    close(sock);
    unlink(TIME_SRV_PATH);
    _Exit(EXIT_FAILURE);
}

int main(void) {
    ssize_t err, n;
    time_t ticks;
    int cliport;
    struct hostent *vm;
    struct sockaddr_un name;

    struct in_addr vm_ip;
    char buff[BUFF_SIZE];
    char cli_ip_buff[INET_ADDRSTRLEN];
    char hostname[BUFF_SIZE];

    struct sigaction sigact;

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

    /* set up the signal handler for SIGINT ^C */
    sigact.sa_handler = &handle_sigint;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    err = sigaction(SIGINT, &sigact, NULL);
    if(err < 0) {
        _ERROR("%s: %m\n", "sigaction");
        exit(EXIT_FAILURE);
    }

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
