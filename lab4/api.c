#include "api.h"

/*
* Translate a destination IP to a outgoing hardware address (MAC and interface
* index).
* 1. Connects to the ARP process.
* 2. Sends the IP in IPaddr.
* 3. Recv's (with a timeout) to fill in the HWaddr struct.
*
* RETURNS: 0 on success, -1 on failure with perror printed.
*/
int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    int unixfd, erri;
    ssize_t n, tot_n = 0, errs;
    struct sockaddr_un arp_addr;
    socklen_t arp_len;
    fd_set rset;
    struct timeval tm;

    if(sockaddrlen != sizeof(struct sockaddr_in)) { /* we only support AF_INET */
        _ERROR("Only AF_INET (%d) addresses are supported, not: %d\n", AF_INET, IPaddr->sa_family);
        return -1;
    }
    printf("areq for IP: %s\n", inet_ntoa(((struct sockaddr_in*)IPaddr)->sin_addr));

    unixfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(unixfd < 0) {
        _ERROR("%s: %m\n", "socket()");
        return -1;
    }

    arp_addr.sun_family = AF_LOCAL;
    strncpy(arp_addr.sun_path, ARP_PATH, sizeof(arp_addr.sun_path));
    /* NULL term to be safe */
    arp_addr.sun_path[sizeof(arp_addr.sun_path) - 1] = '\0';

    arp_len = sizeof(struct sockaddr_un);
    erri = connect(unixfd, (struct sockaddr*)&arp_addr, arp_len);
    if(erri < 0) {
        _ERROR("%s: %m\n", "connect()");
        return -1;
    }

    /* todo: create the request to send to ARP */
    errs = write_n(unixfd, &((struct sockaddr_in*)IPaddr)->sin_addr, sizeof(struct in_addr));
    if(errs < 0) {
        _ERROR("%s: %m\n", "write()");
        return -1;
    }

    FD_ZERO(&rset);
    FD_SET(unixfd, &rset);
    tm.tv_sec = 1;
    tm.tv_usec = 0;

    erri = select(unixfd + 1, &rset, NULL, NULL, &tm);
    if(erri < 0) {
        _ERROR("%s: %m\n", "select()");
        return -1;
    }
    if(erri == 0) {
        errno = ETIMEDOUT;
        _ERROR("%s: %m\n", "areq()");
        return -1;
    }
    do {
        /* todo: maybe need to change the read? */
        n = read(unixfd, (tot_n + &HWaddr), (sizeof(struct hwaddr) - tot_n));
        if(n < 0) {
            if(errno == EINTR)
                continue;
            _ERROR("%s: %m\n", "read()");
            return -1;
        } else if(n == 0) { /* the socket was closed by the server */
            errno = ECONNRESET;
            _ERROR("%s: %m\n", "read()");
            return -1;
        }
        tot_n += n;
    } while (n > 0);

    printf("areq found: ");
    print_hwa(HWaddr->sll_addr, 6);
    printf("\n");

    close(unixfd);
    return 0;
}