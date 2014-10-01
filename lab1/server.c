#include "server.h"

int main(void) {

    fd_set fdset;
    struct timeval time;
    int newfd;
    socklen_t addrlen;

    struct sockaddr_in timesrv;
    struct sockaddr_in echosrv;
    struct thread_args targs;
    int echo_listen_fd;
    int time_listen_fd;
    int err;
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

    if(echo_listen_fd < 3 || time_listen_fd < 3) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    //bind
    err = bind(echo_listen_fd,(struct sockaddr *)&echosrv,sizeof(echosrv));
    if(err != 0) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    err = bind(time_listen_fd,(struct sockaddr *)&timesrv,sizeof(timesrv));
    if(err != 0) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    //listen
    err = listen(echo_listen_fd, BACKLOG);
    if(err != 0) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    err = listen(time_listen_fd, BACKLOG);
    if(err != 0) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    for(;;) {
        time.tv_sec = 10;
        time.tv_usec = 0;

        FD_ZERO(&fdset);
        FD_SET(STDIN_FILENO, &fdset);
        FD_SET(echo_listen_fd, &fdset);
        FD_SET(time_listen_fd, &fdset);

        err = select(time_listen_fd + 1, &fdset, NULL, NULL, &time);
        if(err < 0) {
            perror("select()");

        }

        if (FD_ISSET(echo_listen_fd, &fdset)) {
            printf("setting up a echo server\n");
            addrlen = sizeof(echosrv);
            newfd = accept(echo_listen_fd, (struct sockaddr *) &echosrv, &addrlen);
            targs.connfd = newfd;

            int iret1 = pthread_create(&thread, NULL, (void *) &echo_server, &targs);
            if (iret1) {
                fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(time_listen_fd, &fdset)) {
            printf("setting up a time server\n");
            addrlen = sizeof(timesrv);
            newfd = accept(time_listen_fd, (struct sockaddr *) &echosrv, &addrlen);
            targs.connfd = newfd;

            int iret1 = pthread_create(&thread, NULL, (void *) &time_server, &targs);
            if (iret1) {
                fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &fdset)) {
            while ( getchar() != '\n' );
            printf("Don't type things here\n");
        }
    }

    return EXIT_SUCCESS;
}

void echo_server(struct thread_args *targs) {
    int n = 1;
    char buffer[BUFF_SIZE];

    while(n > 0) {
        n = recv(targs->connfd, buffer, BUFF_SIZE, 0);
        buffer[n] = 0;
        send(targs->connfd, buffer, n, 0);
    }
    printf("closing the echo server\n");
    close(targs->connfd);

}

void time_server(struct thread_args *targs) {

    char *word = "hello\n";

    while(1) {
        send(targs->connfd, word, strlen(word), 0);
        sleep(5);
    }
    printf("closing the time server\n");
    close(targs->connfd);

}
