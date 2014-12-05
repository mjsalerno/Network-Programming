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
    ssize_t n, errs;
    size_t tot_n = 0;
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
    errs = send(unixfd, &((struct sockaddr_in*)IPaddr)->sin_addr, sizeof(struct in_addr), 0);
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
        n = read(unixfd, (tot_n + HWaddr), (sizeof(struct hwaddr) - tot_n));
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
    } while (n > 0 && tot_n < sizeof(struct hwaddr));
    _DEBUG("ARP sent me %d bytes.\n", (int)tot_n);
    print_hwaddr(HWaddr);
    printf("areq found dst mac: ");
    print_hwa(HWaddr->dst_sll_addr, HWaddr->dst_sll_halen);
    printf("\n");

    close(unixfd);
    return 0;
}

void print_hwaddr(struct hwaddr* addr) {

    /*
    int            src_sll_ifindex;	  Interface number
    unsigned short dst_sll_hatype;	  Hardware type
    unsigned char  dst_sll_halen;	  Length of address
    unsigned char  dst_sll_addr[8];	  Physical layer address
    unsigned short src_sll_hatype;	  Hardware type
    unsigned char  src_sll_halen;	  Length of address
    unsigned char  src_sll_addr[8];	  Physical layer address
    */
#ifdef DEBUG
    printf("======= hwaddr from arp =======\n");
    printf("index     : %8d\n", addr->src_sll_ifindex);
    printf("dst_hatype: %8hu\n", addr->dst_sll_hatype);
    printf("dst_halen : %8d\n", addr->dst_sll_halen);
    printf("dst_addr  : ");
    print_hwa(addr->dst_sll_addr, addr->dst_sll_halen);
    printf("\nsrc_hatype: %8hu\n", addr->src_sll_hatype);
    printf("src_halen : %8d\n", addr->src_sll_halen);
    printf("src_addr  : ");
    print_hwa(addr->dst_sll_addr, addr->dst_sll_halen);
    printf("\n===========================\n");
#endif
}
