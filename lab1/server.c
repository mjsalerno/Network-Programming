#include "server.h"

int main(int argc, char**argv) {

    struct sockaddr_in timesrv;
    struct sockaddr_in echosrv;
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

    int maxfd = 2;
    int fdrdy = 0;
    fd_set fdset;
    struct timeval time;

    time.tv_sec = 10;
    time.tv_usec = 0;
    FD_ZERO(&fdset);
    my_fd_set(&fdset, fds, MAX_FD);

    fdrdy = select(maxfd, &fdset, NULL, NULL, &time);
    printf("this many: %d\n", fdrdy);

    if(FD_ISSET(echo_listen_fd, &fdset)) {
        int iret1 = pthread_create(&thread, NULL, time_server, (void *)echosrv);
        if (iret1) {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}

void time_server(struct sockaddr_in echosrv) {

//    for(;;) {
//        socklen_t clilen;
//        clilen=sizeof(echosrv);
//        connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);
//
//        if ((childpid = fork()) == 0)
//        {
//            close (listenfd);
//
//            for(;;)
//            {
//                n = recvfrom(connfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&clilen);
//                sendto(connfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
//                printf("-------------------------------------------------------\n");
//                mesg[n] = 0;
//                printf("Received the following:\n");
//                printf("%s",mesg);
//                printf("-------------------------------------------------------\n");
//            }
//
//        }
//        close(connfd);
//    }

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