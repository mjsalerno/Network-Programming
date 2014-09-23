
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

void print_ip_dns(const char *str);

int main(int argc, const char **argv) {

    if (argc != 2) {
        (void) printf("usage: %s IP-address\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    print_ip_dns(argv[1]);

    int loopout = 1;
    int loopin = 1;
    char input[5];
    int choice = 0;
    int pid;
    do {

        printf("\nPick a server type (echo/time/quit) : ");

        fgets(input, 5, stdin);
        input[4] = 0;
        while ( getchar() != '\n' );

        if(strcmp(input, "echo") == 0) {
            loopout = 1;
            choice = 1;
        } else if(strcmp(input, "time") == 0) {
            loopout = 1;
            choice = 2;
        } else if(strcmp(input, "quit") == 0) {
            loopout = 0;
            choice = 3;
        } else {
            printf("Not a valid input: %s\n", input);
            choice = 0;
        }

        if(choice == 0) {
            continue;
        }

        pid = fork();

        if(pid < 0) {
            perror("There was an error forking\n");
            exit(EXIT_FAILURE);
        }

        if(pid == 0) {
            do {
                printf("im a child\n");
                loopin = 0;
            } while (loopin);
        }

    } while(loopout);

    exit (EXIT_SUCCESS);
}

void print_ip_dns(const char *str) {
    int fromIp = -1;
    char **p;
    in_addr_t addr;
    struct hostent *hp;

    if ((int)(addr = inet_addr(str)) == -1) {
        hp = gethostbyname(str);
        fromIp = 0;
    } else {
        hp = gethostbyaddr((char *) &addr, 4, AF_INET);
        fromIp = 1;
    }

    if (hp == NULL) {
        printf("host information for %s not found\n", str);
        exit(EXIT_FAILURE);
    }

    for (p = hp->h_addr_list; *p != 0; p++) {
        char **q;

        if(fromIp == 0) {
            struct in_addr in;
            memcpy(&in.s_addr, *p, sizeof(in.s_addr));
            printf("IP: %s\n", inet_ntoa(in));

        } if(fromIp == 1) {
            printf("DNS: %s", hp->h_name);
            for (q = hp->h_aliases; *q != 0; q++) {
                printf(" %s", *q);
            }
            putchar('\n');
        }
    }
}