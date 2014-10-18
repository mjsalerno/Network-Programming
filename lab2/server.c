#include <assert.h>
#include "server.h"

int main(int argc, const char **argv) {
    const char *path;

    unsigned short port;
    fd_set fdset;
    long window;
    int pid;
    int err;


    if(argc < 2) {
        path = "./server.in";
        _DEBUG("%s", "no args: default path is ./server.in\n");
    } else {
        path = argv[1];
    }

    /*Read the config*/
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        perror("could not open server config file");
        exit(EXIT_FAILURE);
    }

    /*Get the port*/
    port = (uint16_t) int_from_config(file, "There was an error getting the port number");
    if(port < 1) {
        fprintf(stderr, "The port can not be less than 1\n");
        exit(EXIT_FAILURE);
    } 
    printf("Port: %hu\n", port);
    
    /*Get the window size*/
    window = int_from_config(file, "There was an error getting the window size number");
    if(window < 1) {
        fprintf(stderr, "The window can not be less than 1\n");
        exit(EXIT_FAILURE);
    }
    _DEBUG("Window: %ld\n", window);
    _DEBUG("config: %s\n", path);

    err = fclose(file);
    if(err < 0) {
        perror("server.fclose()");
    }

    int sockfd;
    struct sockaddr_in servaddr;//,cliaddr;
    //socklen_t len;
    //char mesg[BUFF_SIZE];

    sockfd=socket(AF_INET,SOCK_DGRAM,0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    if (err < 0) {
        perror("server.bind()");
        exit(EXIT_FAILURE);
    }

    int i = 1;
    while(i--) {


        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);
        err = select(sockfd + 1, &fdset, NULL, NULL, NULL);
        if (err < 0 || !FD_ISSET(sockfd, &fdset)) {
            perror("server.select()");
            exit(EXIT_FAILURE);
        }

        _DEBUG("%s\n", "a client is connecting");


//    for (;;) {
//        len = sizeof(cliaddr);
//        n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
//        sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
//        mesg[n] = 0;
//        printf("-------------------------------------------------------\n");
//        printf("Received the following: '%s'\n", mesg);
//        printf("-------------------------------------------------------\n");
//    }

        _DEBUG("%s\n", "server.fork()");
        pid = fork();
        _DEBUG("pid: %d\n", pid);
        if (pid < 0) {
            perror("Could not fork()");
            exit(EXIT_FAILURE);
        }

        /*in child*/
        if (pid == 0) {
            child(sockfd);
            /* we should never get here */
            fprintf(stderr, "A child is trying to use the connection select\n");
            assert(0);
        }
    }


    return EXIT_SUCCESS;
}

int child(int parent_sock) {
    struct sockaddr_in servaddr,cliaddr;
    int err;
    int sockfd;
    ssize_t n;
    socklen_t len;
    char msg[BUFF_SIZE];

    _DEBUG("%s", "In child");
    sockfd = socket(AF_INET,SOCK_DGRAM,0);

    bzero(&servaddr, sizeof(servaddr));
    bzero(&cliaddr, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(0);

    /*TODO: pass in ip i want*/
    err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("child.bind()");
        exit(EXIT_FAILURE);
    }

    err = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("child.connect()");
        exit(EXIT_FAILURE);
    }

    /* TODO: err = close();*/

    n = recvfrom(parent_sock, msg, BUFF_SIZE, 0, (struct sockaddr *)&cliaddr,&len);
    if(n < 0) {
        perror("child.recvfrom()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    msg[n] = 0;
    printf("msg in child: %s\n", msg);
    close(sockfd);
    exit(EXIT_SUCCESS);
}
