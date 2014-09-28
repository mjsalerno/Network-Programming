#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 9877
#define BUFF_SIZE 1024

int main(int argc, char**argv) {
    int sockfd,n;
    struct sockaddr_in servaddr, cliaddr;
    char sendline[BUFF_SIZE];
    char recvline[BUFF_SIZE];
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
        exit(EXIT_FAILURE);
    }

    while(!feof(stdin)) {

        while (fgets(sendline, BUFF_SIZE, stdin) != NULL) {

            sendto(sockfd, sendline, strlen(sendline), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
            n = recvfrom(sockfd, recvline, BUFF_SIZE, 0, NULL, NULL);
            recvline[n] = 0;
            fputs(recvline, stdout);
        }
    }

    return EXIT_SUCCESS;
}