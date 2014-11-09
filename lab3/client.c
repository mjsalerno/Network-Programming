#include <netdb.h>
#include <zzip/zzip.h>
#include "client.h"

int main(int argc, char *argv[]) {
    int sockfd, filefd, err;
    socklen_t len;
    struct sockaddr_un my_addr, name_addr, srv_addr;
    socklen_t socklen = 0;
    char fname[] = "socket_client_XXXXXX";  /* template for mkstemp */
    char srvname[BUFF_SIZE] = {0};
    char hostname[BUFF_SIZE] = {0};
    char buf[BUFF_SIZE] = {0};
    struct hostent *he;
    struct in_addr srv_in_addr;
    char *errc;

    memset(&my_addr, 0, sizeof(my_addr));
    memset(&name_addr, 0, sizeof(name_addr));
    memset(&srv_addr, 0, sizeof(srv_addr));
    /* todo: remove*/
    srv_addr.sun_family = AF_LOCAL;
    strcpy(srv_addr.sun_path, KNOWN_PATH);

    sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if(sockfd < 0) {
        perror("ERROR: socket()");
        goto cleanup;
    }

    filefd = mkstemp(fname);
    if(filefd < 0) {
        perror("ERROR: mkstemp()");
        goto cleanup;
    }
    unlink(fname);		            /* we just use mkstemp to get a filename, so close the file, dumb? */

    my_addr.sun_family = AF_LOCAL;
    strncpy(my_addr.sun_path, fname, sizeof(my_addr.sun_path)-1);

    err = bind(sockfd, (struct sockaddr*) &my_addr, (socklen_t)SUN_LEN(&my_addr));
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
    printf("bound name = %s, returned len = %d\n", name_addr.sun_path, len);

    /* prompts the user to choose one of vm1, ..., vm10 as a server node. */
    printf("Enter server host (vm1,..., vm10):\n> ");
    errc = fgets(srvname, sizeof(srvname), stdin);
    if(errc == NULL) {
        fprintf(stderr, "ERROR: fgets() returned NULL\n");
        goto cleanup;
    }
    errc = strchr(srvname, '\n');
    if(errc != NULL) {
        *errc = 0;              /* replace '\n' with NULL term.*/
    }
    /* don't check if's a "vmXX", just call gethostbyname() */
    he = gethostbyname(srvname);
    if(he == NULL) {
        herror("ERROR: gethostbyname()");
        goto cleanup;
    }
    /* take out the first addr from the h_addr_list */
    srv_in_addr = **((struct in_addr**)(he->h_addr_list));
    printf("The server host is %s at %s\n", srvname, inet_ntoa(srv_in_addr));

    err = gethostname(hostname, sizeof(hostname));
    if(err < 0) {
        perror("ERROR: gethostname()");
        goto cleanup;
    }

    /* todo: msg_send */
    err = (int) sendto(sockfd, "H", 2, 0, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if(err < 0) {
        perror("ERROR: sendto()");
        goto cleanup;
    }
    printf("client at node %s sending request to server at %s\n", hostname, srvname);

    _DEBUG(stdout, "");
    /* todo: msg_recv */
    socklen = sizeof(srv_addr);
    err = (int) recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr*)&srv_addr, &socklen);
    if(err < 0) {
        perror("ERROR: sendto()");
        goto cleanup;
    }
    buf[err] = 0;
    printf("RESPONSE: %s\n", buf);

    unlink(fname);		/* OK if this fails */
    exit(EXIT_SUCCESS);
    /* cleanup goto */
cleanup:
    unlink(fname);		/* OK if this fails */
    exit(EXIT_FAILURE);
}
