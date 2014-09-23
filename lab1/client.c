#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, const char **argv) {
    in_addr_t addr;
    struct hostent *hp;
    char **p;
    int fromIp = -1;

    if (argc != 2) {
        (void) printf("usage: %s IP-address\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    if ((int)(addr = inet_addr(argv[1])) == -1) {
        hp = gethostbyname(argv[1]);
        fromIp = 0;
    } else {
        hp = gethostbyaddr((char *) &addr, 4, AF_INET);
        fromIp = 1;
    }

    if (hp == NULL) {
        printf("host information for %s not found\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    for (p = hp->h_addr_list; *p != 0; p++) {
        struct in_addr in;
        char **q;

        if(fromIp == 0) {
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

    exit (EXIT_SUCCESS);
}