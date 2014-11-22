#include "client.h"

static char fname[] = TIME_CLI_PATH;  /* template for mkstemp */
static int sockfd;

void handle_sigint(int sign) {
    /**
    * From signal(7):
    *
    * POSIX.1-2004 (also known as POSIX.1-2001 Technical Corrigendum 2) requires an  implementation
    * to guarantee that the following functions can be safely called inside a signal handler:
    * ... _Exit() ... close() ... unlink() ...
    */
    sign++; /* for -Wall -Wextra -Werror */
    close(sockfd);
    unlink(fname);
    _Exit(EXIT_FAILURE);
}

int main(void) {
    int filefd, err;
    socklen_t len;
    struct sockaddr_un my_addr, name_addr;
    char srvname[BUFF_SIZE] = {0}, prev_srvname[BUFF_SIZE] = {0};
    char hostname[BUFF_SIZE];
    char buf[BUFF_SIZE];
    char ip_buf[INET_ADDRSTRLEN];
    int port;
    struct hostent *he;
    struct in_addr srv_in_addr;
    char *errc;
    struct sigaction sigact;

    fd_set rset;            /* select(2) vars for a timeout */
    struct timeval tv;      /* select(2) vars for a timeout */

    memset(&my_addr, 0, sizeof(my_addr));
    memset(&name_addr, 0, sizeof(name_addr));

    err = gethostname(hostname, sizeof(hostname));  /* get my hostname */
    if(err < 0) {
        perror("ERROR: gethostname()");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if(sockfd < 0) {
        perror("ERROR: socket()");
        exit(EXIT_FAILURE);
    }

    filefd = mkstemp(fname);
    if(filefd < 0) {
        perror("ERROR: mkstemp()");
        goto cleanup;
    }
    /* we just use mkstemp to get a filename, so close the file, dumb right? */
    close(filefd);
    unlink(fname);

    /* set up the signal handler for SIGINT ^C */
    sigact.sa_handler = &handle_sigint;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    err = sigaction(SIGINT, &sigact, NULL);
    if(err < 0) {
        _ERROR("%s: %m\n", "sigaction");
        exit(EXIT_FAILURE);
    }

    my_addr.sun_family = AF_LOCAL;
    strncpy(my_addr.sun_path, fname, sizeof(my_addr.sun_path)-1);

    err = bind(sockfd, (struct sockaddr*) &my_addr, (socklen_t)sizeof(my_addr));
    if(err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    len = sizeof(name_addr);
    err = getsockname(sockfd, (struct sockaddr*) &name_addr, &len);
    if(err < 0) {
        perror("ERROR: getsockname()");
        goto cleanup;
    }
    printf("socket --> %s\n", name_addr.sun_path);

    for(EVER) {

        /* prompts the user to choose one of vm1, ..., vm10 as a server node. */
        printf("Enter server host (vm1, vm2, ..., vm10 or ^D to quit):\n> ");
        errc = fgets(srvname, sizeof(srvname), stdin);
        if (errc == NULL) {
            if(ferror(stdin)) {
                fprintf(stderr, "ERROR: fgets() returned NULL\n");
                goto cleanup;
            } else {
                printf("Read ^D or EOF, terminating...\n");
                break;
            }
        }
        errc = strchr(srvname, '\n');
        if (errc != NULL) {
            *errc = 0;              /* replace '\n' with NULL term.*/
        }
        if(strlen(srvname) == 0) {
            /* if the user only pressed enter, then use the last hostname */
            strncpy(srvname, prev_srvname, sizeof(srvname));
        } else {
            /* user entered new hostname, */
            strncpy(prev_srvname, srvname, sizeof(prev_srvname));
        }

        /* don't check if's a "vmXX", just call gethostbyname() */
        he = gethostbyname(srvname);
        if (he == NULL) {
            herror("ERROR: gethostbyname()");
            if(h_errno == NO_RECOVERY)
                goto cleanup;
            else
                continue;
        }
        /* take out the first addr from the h_addr_list */
        srv_in_addr = **((struct in_addr **) (he->h_addr_list));
        printf("The server host is %s at %s\n", srvname, inet_ntoa(srv_in_addr));

        /* todo: if this is the 1st timeout, then msg_send rediscovery flag */
        err = (int)msg_send(sockfd, inet_ntoa(srv_in_addr), TIME_PORT, "H", 1, 1);
        if (err < 0) {
            perror("ERROR: msg_send()");
            goto cleanup;
        }
        printf("client at node %s: SENDING request to server at %s\n", hostname, srvname);

        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        _DEBUG("waiting for reply, timeout is %ld secs....\n", (long) tv.tv_sec);
        err = select(sockfd + 1, &rset, NULL, NULL, &tv);
        if (err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if (err == 0) {
            /* timeout! */
            _NOTE("client at node %s: TIMEOUT on response from %s\n", hostname, srvname);
            /* todo: set a flag for first timeout */
            continue;

        } else if (FD_ISSET(sockfd, &rset)) {
            err = (int) msg_recv(sockfd, buf, sizeof(buf), ip_buf, &port);
            if (err < 0) {
                perror("ERROR: msg_recv()");
                goto cleanup;
            }
            buf[err] = 0;
            printf("client at node %s: RECEIVED from %s %s\n", hostname, srvname, buf);
        }
    }

    close(sockfd);
    unlink(fname);		/* OK if this fails */
    exit(EXIT_SUCCESS);
    /* cleanup goto */
cleanup:
    close(sockfd);
    unlink(fname);		/* OK if this fails */
    exit(EXIT_FAILURE);
}
