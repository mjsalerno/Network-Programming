#include "server.h"

int main(void) {

    struct sockaddr_in timesrv;
    struct sockaddr_in echosrv;
    struct thread_args targs;
    int echo_listen_fd;
    int time_listen_fd;
    int fds[MAX_FD] = {0};
    pthread_t thread;

    //zero out struct
    bzero(&timesrv,sizeof(timesrv));
    bzero(&echosrv,sizeof(echosrv));

    //fill in struct
    timesrv.sin_family = AF_INET;
    timesrv.sin_addr.s_addr=htonl(INADDR_ANY);
    timesrv.sin_port=htons(TIME_PORT);

    echosrv.sin_family = AF_INET;
    echosrv.sin_addr.s_addr=htonl(INADDR_ANY);
    echosrv.sin_port=htons(ECHO_PORT);

    //get a scket
    echo_listen_fd = socket(AF_INET,SOCK_STREAM,0);
    time_listen_fd = socket(AF_INET,SOCK_STREAM,0);

    //bind
    bind(echo_listen_fd,(struct sockaddr *)&echosrv,sizeof(echosrv));
    bind(time_listen_fd,(struct sockaddr *)&timesrv,sizeof(timesrv));

    //listen
    listen(echo_listen_fd, BACKLOG);
    listen(time_listen_fd, BACKLOG);

    int fdrdy = 0;
    fd_set fdset;
    struct timeval time;
    int newfd;
    socklen_t addrlen;

    time.tv_sec = 10;
    time.tv_usec = 0;

    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO  , &fdset);
    FD_SET(echo_listen_fd, &fdset);
    FD_SET(time_listen_fd, &fdset);

    fdrdy = select(2, &fdset, NULL, NULL, &time);
    printf("this many fds are ready: %d\n", fdrdy);

    if(FD_ISSET(echo_listen_fd, &fdset)) {
        printf("setting up a echo server\n");
        addrlen = sizeof(echosrv);
        newfd = accept(echo_listen_fd, (struct sockaddr *)&echosrv, &addrlen);
        targs.connfd = newfd;

        int iret1 = pthread_create(&thread, NULL, &time_server, &targs);
        if (iret1) {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
            exit(EXIT_FAILURE);
        }
    }

    if(FD_ISSET(time_listen_fd, &fdset)) {
        printf("setting up a time server\n");
    }

    if(FD_ISSET(STDIN_FILENO, &fdset)) {
        printf("Don't type things here\n");
    }

    return EXIT_SUCCESS;
}

void time_server(struct thread_args *targs) {
    int n = 1;
    char buffer[BUFF_SIZE];

    while(n > 0) {
        n = recv(targs->connfd, buffer, BUFF_SIZE, 0);
        buffer[n] = 0;
        send(targs->connfd, buffer, strlen(buffer), 0);
    }

    close(targs);

}

int max(int a, int b) {
    return a > b ? a : b;
}

void my_fd_set(fd_set *fdset, int *arr, int size) {
    int i;

    for(i = 0; i < size; i++) {
        if(*(arr + i)) {
            printf("setting: %d\n", *(arr + i));
            FD_SET(i, fdset);
        }
    }
}