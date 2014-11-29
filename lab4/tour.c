#include "tour.h"

int main(/*int argc, char *argv[]*/) {
    return 0;
}

/*
* Connects to the ARP process to fill out the HWaddr struct with the hardware
* address and outgoing interface index.
*
* RETURNS: 0 on success, -1 on failure with errno containing the error code.
*/
int areq(struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr) {
    int unixfd, erri;
    ssize_t n, tot_n = 0, errs;
    struct sockaddr_un arp_addr;
    socklen_t arp_len;
    char buf[512];
    fd_set rset;
    struct timeval tm;

    if(sockaddrlen != sizeof(struct sockaddr_in)) {
        errno = EINVAL; /* we only support AF_INET */
        return -1;
    }
    printf("areq for IP: %s\n", inet_ntoa(((struct sockaddr_in*)IPaddr)->sin_addr));

    unixfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(unixfd < 0)
        return -1;

    arp_addr.sun_family = AF_LOCAL;
    strncpy(arp_addr.sun_path, ARP_PATH, sizeof(arp_addr.sun_path));
    /* NULL term to be safe */
    arp_addr.sun_path[sizeof(arp_addr.sun_path) - 1] = '\0';

    arp_len = sizeof(struct sockaddr_un);
    erri = connect(unixfd, (struct sockaddr*)&arp_addr, arp_len);
    if(erri < 0)
        return -1;

    /* todo: create the request to send to ARP */
    errs = write_n(unixfd, buf, 512);
    if(errs < 0)
        return -1;

    FD_ZERO(&rset);
    FD_SET(unixfd, &rset);
    tm.tv_sec = 1;
    tm.tv_usec = 0;

    erri = select(unixfd + 1, &rset, NULL, NULL, &tm);
    if(erri < 0)
        return -1;
    if(erri == 0) {
        errno = ETIMEDOUT;
        /* timedout will printout when perror */
        return -1;
    }
    do {
        /* todo: maybe need to change the read? */
        n = read(unixfd, (tot_n + &HWaddr), (sizeof(struct hwaddr) - tot_n));
        if(n < 0) {
            if(errno == EINTR)
                continue;
            return -1;
        }
        tot_n += n;
    } while (n > 0);

    printf("areq found: ");
    print_hwa((char*)HWaddr->sll_addr, 6);
    printf("\n");

    return 0;
}
