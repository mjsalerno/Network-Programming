#include "tour.h"

/* prototypes for private funcs */
int initiate_tour(int argc, char *argv[]);
int handle_first_msg(struct ip *ip_pktbuf, size_t ip_pktlen);
int handle_other_msgs(struct ip *ip_pktbuf, size_t ip_pktlen, int isvalid);
int initiate_shutdown();
int shutdown_tour(void);
ssize_t send_tourmsg(void *ip_pktbuf, size_t ip_pktlen);
int init_multicast_sock(struct tourhdr *firstmsg);

/* sockets */
static int pgrecver; /* PF_INET RAW -- receiving pings replies */
static int pgsender; /* PF_PACKET RAW -- sending ping echo requests */
/* PF_INET RAW HDR_INCLD -- "route traversal" recv/sending tour messages */
static int rtsock;
static int mcaster; /* multicast -- recv/sending group messages */

static struct sockaddr_in mcast_addr;

static char host_name[128];
static struct in_addr host_ip;

/**
* Sets up the sockets then calls initiate_tour() and handle_first_msg()
*/
int main(int argc, char *argv[]) {
    const int on = 1;
    int erri;
    int send_initial_msg = 0;
    char ipbuf[IP_MAXPACKET]; /* fixme: too big or just right? */

    if(argc > 1) {
        /* invoked with args so we should send the first msg. */
        send_initial_msg = 1;
    }

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

    if(send_initial_msg) {
        erri = initiate_tour(argc, argv);
        if (erri < 0) {
            goto cleanup;
        }
    }

    if(!send_initial_msg) {
        erri = handle_first_msg((struct ip*)ipbuf, sizeof(ipbuf));
        if (erri < 0) {
            _ERROR("%s: %m\n", "handle_first_msg()");
            goto cleanup;
        }
    }

    erri = handle_other_msgs((struct ip*)ipbuf, sizeof(ipbuf), !send_initial_msg);
    if(erri < 0) {
        _ERROR("%s: %m\n", "handle_other_msgs()");
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
int initial_tour_msg(void *tourhdrbuf, char *vms[], int n) {
    int i;
    struct tourhdr *hdrp;   /* base of tourhdrbuf */
    struct in_addr *curr_ip; /* points into tourhdrbuf */
    struct in_addr prev_ip, tmp_ip;
    struct hostent *he;

    hdrp = (struct tourhdr*) tourhdrbuf;
    inet_pton(AF_INET, TOUR_GRP_IP, &hdrp->g_ip);
    hdrp->g_port = TOUR_GRP_PORT;
    hdrp->index = 0;
    hdrp->num_ips = (uint32_t)n;

    /* get the address of the first ip to store into tourhdrbuf*/
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
    void *tourhdrp; /* points into ip_pktbuf */
    size_t trhdrlen, ip_pktlen;
    int erri;
    ssize_t errs;

    if(argc <= 1) {
        return -1;
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
    tourhdrp = ((char*)ip_pktbuf) + IP4_HDRLEN;

    erri = initial_tour_msg(tourhdrp, (argv + 1), (argc - 1));
    if (erri < 0) {
        _ERROR("%s", "args must be list of: vmXX, vmYY, vmZZ,...\n");
        goto cleanup;
    }

    erri = init_multicast_sock(tourhdrp);
    if(erri < 0)
        goto cleanup;

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

    _DEBUG("%s\n", "Sending a tour msg.");
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
* Monitors only the rtsock for the first msg.
* Joins the multicast group.
* Sends the tour msg to the next node.
*/
int handle_first_msg(struct ip *ip_pktbuf, size_t ip_pktlen) {
    int erri;
    ssize_t errs;
    struct tourhdr *trhdrp;
    socklen_t slen;
    struct sockaddr_in srcaddr;
    fd_set rset;

    slen = sizeof(srcaddr);
    memset(&srcaddr, 0, slen);
    FD_ZERO(&rset);
    for(EVER) {
        FD_SET(rtsock, &rset);
        _DEBUG("%s\n", "Waiting to recv the first tour msg...");
        erri = select((rtsock + 1), &rset, NULL, NULL, NULL);
        _DEBUG("select() returned: %d, fd of rtsock is: %d\n", erri, rtsock);
        if(erri < 0) {
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        if(FD_ISSET(rtsock, &rset)) {
            _DEBUG("%s\n", "Recv'ing on rtsock the first tour msg...");
            errs = recvfrom(rtsock, ip_pktbuf, ip_pktlen, 0, (struct sockaddr*)&srcaddr, &slen);
            if(errs < 0) {
                _ERROR("%s: %m\n", "recvfrom()");
                return -1;
            }
            if((validate_ip_tour(ip_pktbuf, (size_t)errs, &srcaddr)) < 0) {
                continue; /* ignore invalid packets */
            }
            trhdrp = EXTRACT_TOURHDRP(ip_pktbuf);
            erri = init_multicast_sock(trhdrp);
            if(erri < 0) {
                return -1;
            }
            /* fixme: start pinging prev ip*/
            errs = send_tourmsg(ip_pktbuf, (size_t)errs);
            if(errs < 0) {
                _ERROR("%s: %m\n", "send_tourmsg()");
                return -1;
            }
            return 0;
        }
        return -1; /* should never be reached */
    }
}

/**
* Monitors the rtsock and the multicast socket.
*
* ip_pktbuf of size ip_pktlen. It has been filled in (from a call to
* handle_first_msg) only if isvald is 1.
*/
int handle_other_msgs(struct ip *ip_pktbuf, size_t ip_pktlen, int isvalid) {
    struct tourhdr *trhdrp = EXTRACT_TOURHDRP(ip_pktbuf);
    int erri, maxfpd1;
    ssize_t errs;
    socklen_t slen;
    struct sockaddr_in srcaddr;
    fd_set rset;

    slen = sizeof(srcaddr);
    memset(&srcaddr, 0, slen);
    FD_ZERO(&rset);
    maxfpd1 = MAX(rtsock, mcaster) + 1;
    for(EVER) {
        if(isvalid && TOUR_IS_OVER(trhdrp)) {
            initiate_shutdown();
        }

        FD_SET(rtsock, &rset);
        FD_SET(mcaster, &rset);
        _DEBUG("%s\n", "Waiting to recv the a tour msg...");
        erri = select(maxfpd1, &rset, NULL, NULL, NULL);
        _DEBUG("select() returned: %d, fd of rtsock is: %d\n", erri, rtsock);
        if(erri < 0) {
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        if(FD_ISSET(rtsock, &rset)) {
            _DEBUG("%s\n", "Recv'ing on rtsock the first tour msg...");
            errs = recvfrom(rtsock, ip_pktbuf, ip_pktlen, 0, (struct sockaddr*)&srcaddr, &slen);
            if(errs < 0) {
                _ERROR("%s: %m\n", "recvfrom()");
                return -1;
            }
            if((validate_ip_tour(ip_pktbuf, (size_t)errs, &srcaddr)) < 0) {
                continue; /* ignore invalid packets */
            }
            trhdrp = EXTRACT_TOURHDRP(ip_pktbuf);
            erri = init_multicast_sock(trhdrp);
            if(erri < 0) {
                return -1;
            }
            errs = send_tourmsg(ip_pktbuf, (size_t)errs);
            if(errs < 0) {
                _ERROR("%s: %m\n", "send_tourmsg()");
                return -1;
            }
            return 0;
        }
        return -1; /* should never be reached */
    }

    return 0;
}

int initiate_shutdown() {
    /*struct timeval tm;
    tm.tv_sec = 5;
    tm.tv_usec = 200000;
    */

    /* We are the last stop on the route */
    /* initiate Tour shutdown by sending on multicast socket */
    /* fixme: send on mcaster, make function initiate_shutdown()*/
    /*erri = select();*/
    return 0;
}

/**
* Shutdown the tour by sending the multicast message shutdown msg.
* Call from handle_first_msg(), then return to recv your own message.
*/
int shutdown_tour(void) {
    return 0;
}

/**
* Called when Tour receives its first tour msg.
* (Also when the first node sends the initial msg)
* NOTE: firstmsg should be in network order
*/
int init_multicast_sock(struct tourhdr *firstmsg) {
    struct sockaddr_in bindaddr;
    struct group_req greq;
    const unsigned char ttl = 1;
    int erri;
    _DEBUG("%s\n", "Initializing the multicast socket...");
    /* fill out the multicast address (which we'll join) */
    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_addr.s_addr = firstmsg->g_ip.s_addr;
    mcast_addr.sin_port = firstmsg->g_port;
    /* bind to INADDR_ANY, fine as long as we validate incoming pkts */
    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = mcast_addr.sin_port;
    _DEBUG("%s\n", "Binding mcast socket...");
    erri = bind(mcaster, (struct sockaddr*)&bindaddr, sizeof(bindaddr));
    if(erri < 0) {
        _ERROR("%s: %m\n", "bind()");
        return -1;
    }

    /* let the kernel pick the interface index */
    /* todo: if needed setsockopt(IP_MULTICAST_IF, eth0) */
    greq.gr_interface = 0;
    memcpy(&greq.gr_group, &mcast_addr, sizeof(struct sockaddr_in));
    _DEBUG("%s\n", "Joining the mcast group...");
    erri = setsockopt(mcaster, IPPROTO_IP, MCAST_JOIN_GROUP, &greq, sizeof(greq));
    if(erri < 0) {
        _ERROR("%s: %m\n", "setsockopt(MCAST_JOIN_GROUP)");
        return -1;
    }
    /* TTL of 1 should be the default, but let's set it anyway */
    _DEBUG("%s\n", "Setting the mcast TTL to 1...");
    erri = setsockopt(mcaster, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    if(erri < 0) {
        _ERROR("%s: %m\n", "setsockopt(IP_MULTICAST_TTL)");
        return -1;
    }
    /* setsockopt(IP_MULTICAST_LOOP) is enabled per RFC 1122 */
    return 0;
}

/* Returns -1 if invalid or 0 if valid. */
int validate_ip_tour(struct ip *ip_pktp, size_t n, struct sockaddr_in *srcaddr) {
    struct tourhdr *trhdrp;
    if(n < TOURHDR_MIN) {
        _DEBUG("%s\n", "msg too small. Ignoring....");
        return -1;
    }
    if(ip_pktp->ip_id != TOUR_IP_ID) {
        _DEBUG("msg ip ID: %hhu, TOUR_IP_ID: %hhu. Don't match, ignoring....",
                ip_pktp->ip_id, TOUR_IP_ID);
        return -1;
    }
    /* point past the ip header to the tour message */
    trhdrp = EXTRACT_TOURHDRP(ip_pktp);
    if(TOUR_PREV(trhdrp)->s_addr != srcaddr->sin_addr.s_addr) {
        _DEBUG("%s\n", "Prev ip in msg != src ip of msg. Ignoring....");
        return -1;
    }
    if(TOUR_CURR(trhdrp)->s_addr != htonl(host_ip.s_addr)) {
        _DEBUG("%s\n", "Curr ip in msg != host ip. Ignoring....");
        return -1;
    }
    return 0;
}

int validate_mcast() {
    return 0;
}

int validate_icmp() {
    return 0;
}
