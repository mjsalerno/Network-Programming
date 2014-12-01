#include "tour.h"

/* prototypes for private funcs */
int socket_pf_raw(int proto);
int socket_ip_raw(int proto);
int init_tour_msg(void *hdrbuf, char *vms[], int n);
void ping(void *null);
int handle_tour(void);

/* sockets */
static int pgrecver; /* PF_INET RAW -- receiving pings replies */
static int pgsender; /* PF_PACKET RAW -- sending ping echo requests */
static int rtsock; /* PF_INET RAW HDR_INCLD -- recv/sending tour messages */

char host_name[128];
struct in_addr host_ip;

int main(int argc, char *argv[]) {
    int erri, startedtour = 0;
    char *hdrbuf = NULL;

    erri = gethostname_ip(host_name, &host_ip);
    if(erri < 0) {
        exit(EXIT_FAILURE);
    }

    if (argc > 1) {
        /* if we have been invoked to start/initiate the tour */
        startedtour = 1;
    }

    /* create the two ping sockets and the tour (rt) socket */
    pgsender = socket_pf_raw(ETH_P_IP);
    rtsock = socket_ip_raw(IPPROTO_TOUR);
    pgrecver = socket_ip_raw(IPPROTO_ICMP); /* make the ping pg socket for the threads */

    if (startedtour) {               /* We are initiating a new tour. */
        /* We'll have (argc - 1) stops in the tour */
        hdrbuf = malloc(sizeof(struct tourhdr) +
                sizeof(struct in_addr) * (argc - 1));
        erri = init_tour_msg(hdrbuf, (argv + 1), (argc - 1));
        if (erri < 0) {
            perror("ERROR: init_tour_msg()");
            goto cleanup;
        }
        /* call start_tour() */
    }

    /* now handle all messages */
    erri = handle_tour();
    if(erri < 0) {
        goto cleanup;
    }

    // listen for tour msgs on rt socket, ping on pg socket

    if(startedtour)
        free(hdrbuf);
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    exit(EXIT_SUCCESS);
    cleanup:
    if(startedtour)
        free(hdrbuf);
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    exit(EXIT_FAILURE);
}

int handle_tour(void) {
    int err;
    struct hwaddr HWaddr;
    struct sockaddr_in dstaddr;
    socklen_t slen;
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    slen = sizeof(dstaddr);

    err = areq((struct sockaddr*)&dstaddr, slen, &HWaddr);
    if(err < 0) {
        return -1;
    }
    _DEBUG("%s", "Got a mac: ");
    print_hwa(HWaddr.sll_addr, HWaddr.sll_halen);
    printf("\n");
    return 0;
}

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
    char buf[512];
    fd_set rset;
    struct timeval tm;

    if(sockaddrlen != sizeof(struct sockaddr_in)) { /* we only support AF_INET */
        _ERROR("Only AF_INET(%d) addresses are supported, not: %d\n", AF_INET, IPaddr->sa_family);
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
    errs = write_n(unixfd, buf, 512);
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
        /* timedout will printout when perror */
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
        }
        tot_n += n;
    } while (n > 0);

    printf("areq found: ");
    print_hwa(HWaddr->sll_addr, 6);
    printf("\n");

    return 0;
}

/* create an AF_INET, SOCK_RAW socket */
int socket_ip_raw(int proto) {
    int fd;
    fd = socket(AF_INET, SOCK_RAW, proto);
    if(fd < 0) {
        _ERROR("socket(AF_INET, SOCK_RAW, %d): %m\n", proto);
        exit(EXIT_FAILURE);
    }
    return fd;
}

/* create an PF_PACKET, SOCK_RAW socket */
int socket_pf_raw(int proto) {
    int fd;
    fd = socket(PF_PACKET, SOCK_RAW, proto);
    if(fd < 0) {
        _ERROR("socket(PF_PACKET, SOCK_RAW, %d): %m\n", proto);
        exit(EXIT_FAILURE);
    }
    return fd;
}

/**
* Takes list of n hostnames, converts hostnames to ips.
* RETURNS: 0 in success, -1 on failure
* NOTE: hdrbuf is assumed to be larger enough to hold the message.
*/
int init_tour_msg(void *hdrbuf, char *vms[], int n) {
    int i;
    struct tourhdr *hdrp;   /* base of hdrbuf */
    struct in_addr *curr_ip; /* points into hdrbuf */
    struct hostent *he;

    hdrp = (struct tourhdr*)hdrbuf;
    inet_pton(AF_INET, TOUR_GRP_IP, &hdrp->g_ip);
    hdrp->g_port = TOUR_GRP_PORT;
    hdrp->index = 0;
    hdrp->num_ips = (uint32_t)n;

    curr_ip = TOUR_CURR(hdrp);

    for(i = 0; i < n; i++, curr_ip++) {
        /* don't check if's a "vmXX", just call gethostbyname() */
        if(vms[i][0] != 'v' || vms[i][1] != 'm') {
            errno = EINVAL;
            return -1;
        }
        he = gethostbyname(vms[i]);
        if (he == NULL) {
            herror("ERROR: gethostbyname()");
            errno = EINVAL;
            return -1;
        }
        /* take out the first addr from the h_addr_list */
        *curr_ip = **((struct in_addr **) (he->h_addr_list));
        _DEBUG("Tour: %s is %s\n", vms[i], inet_ntoa(*curr_ip));
    }

    return 0;
}
