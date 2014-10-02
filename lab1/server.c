#include "server.h"

int main(void) {

    fd_set fdset;
    int newfd;
    socklen_t addrlen;

    struct sockaddr_in timesrv;
    struct sockaddr_in echosrv;
    struct thread_args targs;
    int echo_listen_fd;
    int time_listen_fd;
    pthread_t thread;
    int err;

    /*zero out struct*/
    bzero(&timesrv,sizeof(timesrv));
    bzero(&echosrv,sizeof(echosrv));

    /*fill in struct*/
    timesrv.sin_family = AF_INET;
    timesrv.sin_addr.s_addr=htonl(INADDR_ANY);
    timesrv.sin_port=htons(TIME_PORT);

    echosrv.sin_family = AF_INET;
    echosrv.sin_addr.s_addr=htonl(INADDR_ANY);
    echosrv.sin_port=htons(ECHO_PORT);

    /*get a socket*/
    echo_listen_fd = socket(AF_INET,SOCK_STREAM,0);
    time_listen_fd = socket(AF_INET,SOCK_STREAM,0);

    if(echo_listen_fd < 3 || time_listen_fd < 3) {
        perror("server.socket()");
        exit(EXIT_FAILURE);
    }

    /*bind*/
    err = bind(echo_listen_fd,(struct sockaddr *)&echosrv,sizeof(echosrv));
    if(err != 0) {
        perror("server.bind(echo)");
        exit(EXIT_FAILURE);
    } else {
        printf("Echo Port: %4d\n", ECHO_PORT);
    }

    err = bind(time_listen_fd,(struct sockaddr *)&timesrv,sizeof(timesrv));
    if(err != 0) {
        perror("server.bind(time)");
        exit(EXIT_FAILURE);
    } else {
        printf("Time Port: %4d\n", TIME_PORT);
    }

    /*listen*/
    err = listen(echo_listen_fd, BACKLOG);
    if(err != 0) {
        perror("server.listen(echo)");
        exit(EXIT_FAILURE);
    }

    err = listen(time_listen_fd, BACKLOG);
    if(err != 0) {
        perror("server.listen(echo)");
        exit(EXIT_FAILURE);
    }

    for(;;) {

        FD_ZERO(&fdset);
        FD_SET(STDIN_FILENO, &fdset);
        FD_SET(echo_listen_fd, &fdset);
        FD_SET(time_listen_fd, &fdset);

        err = select(time_listen_fd + 1, &fdset, NULL, NULL, NULL);
        if(err < 0) {
            perror("select()");
        }

        if (FD_ISSET(echo_listen_fd, &fdset)) {
            int iret1;
            printf("Echo: start\n");
            addrlen = sizeof(echosrv);
            newfd = accept(echo_listen_fd, (struct sockaddr *) &echosrv, &addrlen);
            targs.connfd = newfd;

            iret1 = pthread_create(&thread, NULL, (void *) &echo_server, &targs);
            if (iret1) {
                fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
                exit(EXIT_FAILURE);
            }
        }

        if (FD_ISSET(time_listen_fd, &fdset)) {
            int iret1;
            printf("Time: start\n");
            addrlen = sizeof(timesrv);
            newfd = accept(time_listen_fd, (struct sockaddr *) &echosrv, &addrlen);
            targs.connfd = newfd;

            iret1 = pthread_create(&thread, NULL, (void *) &time_server, &targs);
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
    int fd = targs->connfd;
    int n = 1;
    char buffer[BUFF_SIZE];

    while(n > 0) {
        n = recv(fd, buffer, BUFF_SIZE, 0);
        if(n < 0) {
            perror("echos.recv()");
            close(fd);
            pthread_exit(NULL);
        }
        buffer[n] = 0;
        n = send(fd, buffer, n, 0);
        if(n < 0) {
            perror("echos.send()");
            close(fd);
            pthread_exit(NULL);
        }
    }
    printf("Echo: stop\n");
    close(fd);
    pthread_exit(NULL);
}

void time_server(struct thread_args *targs) {
    int fd = targs->connfd;
    char buff[BUFF_SIZE];
    struct timeval timee;
    fd_set fdset;
    time_t ticks;
    int n;
    int err = 0;

    while( err == 0) {
        timee.tv_sec = 5;
        timee.tv_usec = 0;

        FD_ZERO(&fdset);
        FD_SET(fd, &fdset);

        ticks = time(NULL);
        snprintf(buff, sizeof(buff), "%.24s\r\n", ctime(&ticks));
        n = send(fd, buff, strlen(buff), 0);
        if(n < 1) {
            perror("times.send()");
        }

        err = select(fd + 1, &fdset, NULL, NULL, &timee);
        if(err < 0) {
            perror("times.select()");
        }

    }
    printf("Time: stop\n");
    close(targs->connfd);
    pthread_exit(NULL);

}
