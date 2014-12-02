#include "tour.h"

/* prototypes for private funcs */
int initiate_tour(int argc, char *argv[]);
int process_tour(void);
int shutdown_tour(void);
ssize_t send_tourmsg(void *ip_pktbuf, size_t ip_pktlen);
int init_multicast_sock(in_port_t group_port);

/* sockets */
static int pgrecver; /* PF_INET RAW -- receiving pings replies */
static int pgsender; /* PF_PACKET RAW -- sending ping echo requests */
/* PF_INET RAW HDR_INCLD -- "route traversal" recv/sending tour messages */
static int rtsock;
static int mcaster; /* multicast -- recv/sending group messages */

static char host_name[128];
static struct in_addr host_ip;

/**
* Sets up the sockets then calls initiate_tour() and process_tour()
*/
int main(int argc, char *argv[]) {
    const int on = 1;
    int erri;

    erri = gethostname_ip(host_name, &host_ip);
    if(erri < 0) {
        exit(EXIT_FAILURE);
    }

    /* create the two ping sockets and the tour (rt) socket */
    rtsock = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR);
    if(rtsock < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        exit(EXIT_FAILURE);
    }
    if((setsockopt(rtsock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on))) < 0) {
        _ERROR("%s: %m\n", "setsockopt(IP_HDRINCL)");
        close(rtsock);
        exit(EXIT_FAILURE);
    }
    pgsender = socket(PF_PACKET, SOCK_RAW, ETH_P_IP);
    if(pgsender < 0) {
        _ERROR("%s: %m\n", "socket(PF_PACKET, SOCK_RAW, ETH_P_IP)");
        close(rtsock);
        exit(EXIT_FAILURE);
    }
    pgrecver = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR);
    if(pgrecver < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        close(rtsock);
        close(pgsender);
        exit(EXIT_FAILURE);
    }
    /* create the multicast socket (will be binded/joined later) */
    mcaster = socket(AF_INET, SOCK_DGRAM, 0);
    if(pgrecver < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_DGRAM, 0)");
        close(rtsock);
        close(pgsender);
        close(pgrecver);
        exit(EXIT_FAILURE);
    }

    erri = initiate_tour(argc, argv); /* start tour if needed */
    if(erri < 0) {
        goto cleanup;
    }

    erri = process_tour(); /* now handle all messages */
    if(erri < 0) {
        goto cleanup;
    }

    _DEBUG("%s\n", "Shutting down successfully...");
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    exit(EXIT_SUCCESS);

cleanup:
    _ERROR("%s\n", "Shutting down because of previous failures...");
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    exit(EXIT_FAILURE);
}


/**
* Takes list of n hostnames, converts hostnames to ips.
* Stores list (appended with the host IP) into hdrbuf.
*
* RETURNS: 0 in success, -1 on failure.
* NOTE: hdrbuf is assumed to be larger enough to hold the message.
*/
int init_tour_msg(void *hdrbuf, char *vms[], int n) {
    int i;
    struct tourhdr *hdrp;   /* base of hdrbuf */
    struct in_addr *curr_ip; /* points into hdrbuf */
    struct in_addr prev_ip, tmp_ip;
    struct hostent *he;

    hdrp = (struct tourhdr*)hdrbuf;
    inet_pton(AF_INET, TOUR_GRP_IP, &hdrp->g_ip);
    hdrp->g_port = TOUR_GRP_PORT;
    hdrp->index = 0;
    hdrp->num_ips = (uint32_t)n;

    /* get the address of the first ip to store into hdrbuf*/
    curr_ip = TOUR_CURR(hdrp);
    /* pre-append the list with our own IP */
    curr_ip->s_addr = htonl(host_ip.s_addr);
    prev_ip = *curr_ip;
    curr_ip++;

    for(i = 0; i < n; i++, curr_ip++) {
        /* check it's a "vmXX", then call gethostbyname() */
        if(vms[i][0] != 'v' || vms[i][1] != 'm') {
            errno = EINVAL;
            return -1;
        }
        he = gethostbyname(vms[i]);
        if(he == NULL) {
            herror("ERROR: gethostbyname()");
            errno = EINVAL;
            return -1;
        }
        /* take out the first addr from the h_addr_list */
        tmp_ip = **((struct in_addr **) (he->h_addr_list));
        _DEBUG("Tour Stop #%2d: %s at %s\n", i, vms[i], inet_ntoa(*curr_ip));

        /* Store the address into the tourhdr we're building up. */
        curr_ip->s_addr = htonl(tmp_ip.s_addr);
        if(prev_ip.s_addr == curr_ip->s_addr) {
            _ERROR("Cannot go from %s to %s, they are the same\n", vms[i], vms[i]);
            errno = EINVAL;
            return -1;
        }
        prev_ip = *curr_ip;
    }

    return 0;
}


/**
* If the process was invoked with a list of hostnames then it will send the
* initial tour msg. Otherwise it will just return.
*/
int initiate_tour(int argc, char *argv[]) {
    void *ip_pktbuf; /* |--- ip header ---|--- tour header ---|-- addrs --| */
    void *trhdrbuf; /* points into ip_pktbuf */
    size_t trhdrlen, ip_pktlen;
    int erri;
    ssize_t errs;

    if(argc <= 1) {
        return 0;
    }
    /* Else we are initiating a new tour. */

    /* We'll have (argc - 1) nodes in the tour, +1 for our own ip */
    trhdrlen = sizeof(struct tourhdr) + sizeof(struct in_addr) * argc;
    ip_pktlen = IP4_HDRLEN + trhdrlen;

    ip_pktbuf = malloc(ip_pktlen);
    if(ip_pktbuf == NULL) {
        _ERROR("%s: %m\n", "malloc failed!");
        return -1;
    }
    trhdrbuf = ((char*)ip_pktbuf) + IP4_HDRLEN;

    erri = init_tour_msg(trhdrbuf, (argv + 1), (argc - 1));
    if (erri < 0) {
        _ERROR("%s", "args must be list of: vmXX, vmYY, vmZZ,...\n");
        goto cleanup;
    }

    errs = send_tourmsg(ip_pktbuf, ip_pktlen);
    if(errs < 0) {
        _ERROR("%s: %m\n", "send()");
        goto cleanup;
    }

    free(ip_pktbuf);
    return 0;
cleanup:
    free(ip_pktbuf);
    return -1;
}

/**
* Increment the index in tourhdr (contained in ip_pktbuf).
* Send to the next hop.
*
* ip_pktbuf is space for:
* sizeof(struct ip) + sizeof(struct tourhdr) +  (X * 4) bytes
* |-- struct ip --|-- struct tourhdr --|-- list of X addresses --|
*
* RETURNS: number of bytes sent
*
*/
ssize_t send_tourmsg(void *ip_pktbuf, size_t ip_pktlen) {
    ssize_t n;
    struct in_addr *next_ip;
    struct tourhdr *tourhdrp;

    /* point to the tourhdr inside the ip_pktbuf */
    tourhdrp = (struct tourhdr*)(IP4_HDRLEN + ((char*)ip_pktbuf));

    /* grab the next destination IP, and increment the index */
    next_ip = TOUR_NEXT(tourhdrp);
    tourhdrp->index++;

    craft_ip(ip_pktbuf, host_ip, *next_ip, (ip_pktlen - IP4_HDRLEN));

    n = send(rtsock, ip_pktbuf, ip_pktlen, 0);
    return n;
}

/**
* Handles all of the tour processing (except for sending the initial msg).
* Monitors the rtsock and the multicast socket.
*/
int process_tour(void) {
    int err;
    struct hwaddr HWaddr;
    struct sockaddr_in dstaddr;
    socklen_t slen;

    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = INADDR_LOOPBACK;
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

/**
* Shutdown the tour by sending the multicast message shutdown msg.
* Call from process_tour(), then return to recv your own message.
*/
int shutdown_tour() {
    return 0;
}

/**
* Called when Tour receives its first tour msg.
*/
int init_multicast_sock(in_port_t group_port) {
    struct sockaddr_in bindaddr;
    int erri;

    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = group_port;
    erri = bind(mcaster, (struct sockaddr*)&bindaddr, sizeof(bindaddr));
    if(erri < 0) {
        return -1;
    }
    /**
    *  todo setsockopt(MCAST_JOIN_GROUP)
    *  todo setsockopt(IP_MULTICAST_IF) eth0
    *  todo setsockopt(IP_MULTICAST_TTL) 1
    *  todo setsockopt(IP_MULTICAST_LOOP) on
    */
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
