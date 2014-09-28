#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define PORT 13
#define BUFF_SIZE 1024

int main(int argc, char**argv) {
    int sockfd,n;
    struct sockaddr_in servaddr, cliaddr;
    char sendline[BUFF_SIZE];
    char recvline[BUFF_SIZE];
    char fdbuff[BUFF_SIZE];
    int fd = 0;

    if (argc < 2) {
        printf("usage:  client <IP address>\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 3) {
        fd = atoi(argv[2]);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);
    servaddr.sin_port=htons(PORT);

    if( connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ) {
        perror("Could not connect");
        if(fd > 0) {
            char *error = strerror(errno);
            if(sprintf(fdbuff, "Could not connect: %s\n", error) > 0) {
                write(fd, fdbuff, strlen(fdbuff));
            }
        }
        exit(EXIT_FAILURE);
    }

    while(!feof(stdin)) {

        n = recv(sockfd, recvline, BUFF_SIZE, 0);
        recvline[n] = 0;
        fputs(recvline, stdout);

        if(fd > 0 && n > 0) {
            if(sprintf(fdbuff, "Bytes recieved: %d\n", n) > 0) {
                write(fd, fdbuff, strlen(fdbuff));
            } else {
                perror("Could not write to fdbuff\n");
            }
        }

    }

    close(sockfd);
    close(fd);

    return EXIT_SUCCESS;
}