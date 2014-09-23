#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void print_ip_dns(const char *str);

int main(int argc, const char **argv) {

    if (argc != 2) {
        (void) printf("usage: %s IP-address\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    print_ip_dns(argv[1]);

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
}