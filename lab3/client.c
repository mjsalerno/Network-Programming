#include "client.h"

int main(int argc, char *argv[]) {
    int sockfd, filefd, err;
    socklen_t len;
    struct sockaddr_un my_addr, name_addr, srv_addr;
    char fname[] = TIME_CLI_PATH;  /* template for mkstemp */
    char srvname[BUFF_SIZE] = {0};
    char hostname[BUFF_SIZE] = {0};
    char buf[BUFF_SIZE] = {0};
    char ip_buf[INET_ADDRSTRLEN] = {0};
    int port;
    struct hostent *he;
    struct in_addr srv_in_addr;
    char *errc;

    fd_set rset;            /* select(2) vars for a timeout */
    struct timeval tv;      /* select(2) vars for a timeout */

    memset(&my_addr, 0, sizeof(my_addr));
    memset(&name_addr, 0, sizeof(name_addr));
    memset(&srv_addr, 0, sizeof(srv_addr));
    /* todo: remove*/
    srv_addr.sun_family = AF_LOCAL;
    /* fixme: remove TIME_SRV_PATH */
    strncpy(srv_addr.sun_path, TIME_SRV_PATH, sizeof(my_addr.sun_path)-1);

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
    printf("socket bound to name: %s, len: %d\n", name_addr.sun_path, len);

    for(EVER) {

        /* prompts the user to choose one of vm1, ..., vm10 as a server node. */
        printf("Enter server host (vm1, vm2, ..., vm10):\n> ");
        errc = fgets(srvname, sizeof(srvname), stdin);
        if (errc == NULL) {
            fprintf(stderr, "ERROR: fgets() returned NULL\n");
            goto cleanup;
        }
        errc = strchr(srvname, '\n');
        if (errc != NULL) {
            *errc = 0;              /* replace '\n' with NULL term.*/
        }

        if(*srvname == 'q' || *srvname == 'Q') {
            printf("Bye\n");
            goto cleanup;
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
        err = (int)msg_send(sockfd, inet_ntoa(srv_in_addr), TIME_PORT, "H", 1, 0);
        /*err = (int) sendto(sockfd, "H", 2, 0, (struct sockaddr *) &srv_addr, sizeof(srv_addr));*/
        if (err < 0) {
            perror("ERROR: sendto()");
            goto cleanup;
        }
        printf("client at node %s: sending request to server at %s\n", hostname, srvname);

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
            printf("client at node %s: TIMEOUT on response from %s\n", hostname, srvname);
            /* todo: set a flag for first timeout */
            continue;

        } else if (FD_ISSET(sockfd, &rset)) {
            /* todo: msg_recv */
            err = (int) msg_recv(sockfd, buf, sizeof(buf), ip_buf, &port);
            /*err = (int) recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &srv_addr, &socklen);*/
            if (err < 0) {
                perror("ERROR: sendto()");
                goto cleanup;
            }
            buf[err] = 0;
            printf("RESPONSE: %s\n", buf);
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
