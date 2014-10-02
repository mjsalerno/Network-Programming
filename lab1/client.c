
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

void print_ip_dns(const char *str, char *buff);

int main(int argc, const char **argv) {

    int loopout = 1;
    char input[5];
    int choice = 0;
    pid_t childpid;
    int   stat;
    int fd[2];
    char ipStr[16];
    char fdStr[16];

    if (argc != 2) {
        (void) printf("usage: %s IP-address\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    print_ip_dns(argv[1], ipStr);

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
            exit(EXIT_SUCCESS);

        } else {
            printf("Not a valid input: %s\n", input);
            choice = 0;
            loopout = 1;
            continue;

        }

        if(0 != pipe(fd)) {
            perror("Could not make pipe\n");
            exit(EXIT_FAILURE);
        }

        if ( (childpid = fork()) == 0 && choice != 3) {

            switch (choice) {
                case 1:          /*echo*/

                    if(sprintf(fdStr, "%d", fd[1]) < 1) {
                        perror("Cant convert fd to str");
                        exit(EXIT_FAILURE);
                    }

                    if (execlp("xterm", "xterm", "-e", "./echoc", ipStr, fdStr, (char *) 0) < 0) {
                        printf("Fork failed\n");
                        exit(EXIT_FAILURE);
                    }
                    break;

                case 2:          /*time*/

                    if(sprintf(fdStr, "%d", fd[1]) < 1) {
                        perror("Cant convert fd to str");
                        exit(EXIT_FAILURE);
                    }

                    if (execlp("xterm", "xterm", "-e", "./timec", ipStr, fdStr, (char *) 0) < 0) {
                        printf("Fork failed\n");
                        exit(EXIT_FAILURE);
                    }

                    break;
            }
        } else if(childpid < 0) {
            perror("There was an error forking\n");
            exit(EXIT_FAILURE);

        } else { /*parent*/
            char c;
            close(fd[1]);
            while (read(fd[0], &c, 1) > 0 ) {
                write(STDOUT_FILENO, &c, 1);
            }

            wait(&stat);
        }

    } while(loopout);

    exit (EXIT_SUCCESS);
}

void print_ip_dns(const char *str, char *buff) {
    int fromIp = -1;
    char **p;
    in_addr_t addr;
    struct hostent *hp;

    /*todo: don't use this*/
    if ((int)(addr = inet_addr(str)) == -1) {
        hp = gethostbyname(str);
        fromIp = 0;
    } else {
        hp = gethostbyaddr((char *) &addr, 4, AF_INET);
        strcpy(buff, str);
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
            strcpy(buff, inet_ntoa(in));

        } if(fromIp == 1) {
            printf("DNS: %s", hp->h_name);
            for (q = hp->h_aliases; *q != 0; q++) {
                printf(" %s", *q);
            }
            putchar('\n');
        }
    }
}
