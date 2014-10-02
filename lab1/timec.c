#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include "const.h"

int main(int argc, char**argv) {
    int sockfd,n;
    struct sockaddr_in servaddr;
    fd_set fdset;
    char recvline[BUFF_SIZE];
    char fdbuff[BUFF_SIZE];
    int fd = 0;
    int running = 1;

    if (argc < 2) {
        printf("usage:  client <IP address>\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 3) {
        fd = atoi(argv[2]);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 2) {
        perror("timec.socket()");
        if(fd > 0) {
            char *error = strerror(errno);
            if(sprintf(fdbuff, "timec.socket(): %s\n", error) > 0) {
                write(fd, fdbuff, strlen(fdbuff));
            }
        }
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);
    servaddr.sin_port=htons(TIME_PORT);

    if( connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ) {
        perror("timec.connect()");
        if(fd > 0) {
            char *error = strerror(errno);
            if(sprintf(fdbuff, "timec.connect(): %s\n", error) > 0) {
                write(fd, fdbuff, strlen(fdbuff));
            }
        }
        exit(EXIT_FAILURE);
    }

    while (running) {

        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);

        n = select(sockfd + 1, &fdset, NULL, NULL, NULL);
        if (n < 0) {
            perror("timec.select()");
        }

        if(FD_ISSET(sockfd, &fdset)) {
            n = recv(sockfd, recvline, BUFF_SIZE, 0);
            if(n == 0) {

                if(fd > 0) {
                    if(write(fd, "The servers connection was closed\n", 35) < 1) {
                        perror("timec.write()");
                    }
                }

                running = 0;

            } else if (n < 0) {
                perror("timec.recv()");

            } else {
                recvline[n] = 0;
                fputs(recvline, stdout);

                if(fd > 0) {
                    if(sprintf(fdbuff, "Bytes recieved: %d\n", n) > 0) {
                        write(fd, fdbuff, strlen(fdbuff));

                    } else {
                        perror("Could not write to fdbuff\n");

                    }
                }
            }
        }

    }

    close(sockfd);
    close(fd);

    return EXIT_SUCCESS;
}
