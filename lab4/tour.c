#include "tour.h"
#include "ping.h"

/* prototypes for private funcs */
int initiate_tour(int argc, char *argv[]);
int process_tour(struct ip *ip_pktbuf, size_t ip_pktlen, int mcast_is_enabled);
int initiate_shutdown(void);
ssize_t send_tourmsg(void *ip_pktbuf, size_t ip_pktlen);
int send_mcast(char *msg, size_t msglen);
int init_multicast_sock(struct tourhdr *firstmsg);
void print_tourhdr(struct tourhdr *trhdrp);
int validate_ip_tour(struct ip *ip_pktp, size_t n, struct sockaddr_in *srcaddr);
int validate_mcast(struct msghdr *cmsgp);
int create_ping_recver(void);
int create_ping_sender(struct sockaddr_in *addr);
int stop_pinging(void);
int is_pinging(struct in_addr ipaddr);
int recv_mcast_msg(struct sockaddr_in *srcaddr, socklen_t slen, char *buf, size_t buflen);
#define MSG_NOTVALID -2

/* sockets */
static int pgrecver; /* PF_INET RAW -- receiving pings replies */
static int pgsender; /* PF_PACKET RAW -- sending ping_sender echo requests */
/* PF_INET RAW HDR_INCLD -- "route traversal" recv/sending tour messages */
static int rtsock;
static int mcaster; /* multicast -- recv/sending group messages */

static struct sockaddr_in mcast_addr;

static char host_name[128];
struct in_addr host_ip;
static struct tidhead tidhead;

void handle_sigint(int sign) {
    /**
    * From signal(7):
    * POSIX.1-2004 (also known as POSIX.1-2001 Technical Corrigendum 2) requires an  implementation
    * to guarantee that the following functions can be safely called inside a signal handler:
    * ... _Exit() ... close() ... unlink() ...
    */
    sign++; /* for -Wall -Wextra -Werror */
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    close(mcaster);
    _Exit(EXIT_FAILURE);
}

/**
* Sets up the sockets then calls initiate_tour() and handle_first_msg()
*/
int main(int argc, char *argv[]) {
    const int on = 1;
    int erri;
    int send_initial_msg = 0;
    char ipbuf[IP_MAXPACKET];
    struct sigaction sigact;

    if(argc > 1) {
        /* invoked with args so we should send the first msg. */
        send_initial_msg = 1;
    }

    erri = gethostname_ip(host_name, &host_ip);
    if(erri < 0) {
        exit(EXIT_FAILURE);
    }

    memset(&mcast_addr, 0, sizeof(mcast_addr));

    _DEBUG("%s\n", "Initializing sockets....");
    /* create the two ping_sender sockets and the tour (rt) socket */
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
    pgrecver = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if(pgrecver < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)");
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
    /* we want to use recvmsg() to get the orig dst addr of multicasts */
    erri = setsockopt(mcaster, IPPROTO_IP, IP_RECVORIGDSTADDR, &on, sizeof(on));
    if(erri < 0) {
        _ERROR("%s: %m\n", "setsockopt(IP_RECVORIGDSTADDR)");
        goto cleanup;
    }

    /* set up the signal handler for SIGINT ^C */
    sigact.sa_handler = &handle_sigint;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    erri = sigaction(SIGINT, &sigact, NULL);
    if(erri < 0) {
        _ERROR("%s: %m\n", "sigaction(SIGINT)");
        goto cleanup;
    }

    LIST_INIT(&tidhead); /* init the thread id list */

    erri = create_ping_recver();
    if(erri < 0) {
        goto cleanup;
    }

    if(send_initial_msg) {
        erri = initiate_tour(argc, argv);
        if (erri < 0) {
            goto cleanup;
        }
    }

    erri = process_tour((struct ip *)ipbuf, sizeof(ipbuf), send_initial_msg);
    if(erri < 0) {
        _ERROR("%s: %m\n", "process_tour()");
        goto cleanup;
    }

    _INFO("%s\n", "Shutting down successfully...");
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    close(mcaster);
    exit(EXIT_SUCCESS);

cleanup:
    _ERROR("%s\n", "Shutting down because of previous failures...");
    close(rtsock);
    close(pgsender);
    close(pgrecver);
    close(mcaster);
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
    struct in_addr *curr_ipp; /* points into tourhdrbuf */
    struct in_addr prev_ip, tmp_ip;
    struct hostent *he;

    hdrp = (struct tourhdr*) tourhdrbuf;
    inet_pton(AF_INET, TOUR_GRP_IP, &hdrp->g_ip);
    hdrp->g_ip.s_addr = hdrp->g_ip.s_addr;
    hdrp->g_port = htons(TOUR_GRP_PORT);
    hdrp->index = htons(0);
    hdrp->num_ips = htonl((uint32_t)n);

    /* get the address of the first ip to store into tourhdrbuf*/
    curr_ipp = TOUR_CURR(hdrp);
    /* pre-append the list with our own IP */
    curr_ipp->s_addr = host_ip.s_addr;
    prev_ip = *curr_ipp;
    curr_ipp++;

    for(i = 0; i < n; i++, curr_ipp++) {
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
        _DEBUG("Tour Stop #%2d: %s at %s\n", i, vms[i], inet_ntoa(tmp_ip));

        /* Store the address into the tourhdr we're building up. */
        curr_ipp->s_addr = tmp_ip.s_addr;
        if(prev_ip.s_addr == curr_ipp->s_addr) {
            _ERROR("Cannot go from %s to %s, they are the same\n", vms[i], vms[i]);
            errno = EINVAL;
            return -1;
        }
        prev_ip = *curr_ipp;
    }
    print_tourhdr(hdrp);
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
    _DEBUG("%s\n", "Initiating tour...");
    /* Else we are initiating a new tour. */
    /* We'll have (argc - 1) nodes in the tour, +1 for our own ip */
    trhdrlen = sizeof(struct tourhdr) + sizeof(struct in_addr) * argc;
    ip_pktlen = IP4_HDRLEN + trhdrlen;

    ip_pktbuf = malloc(ip_pktlen);
    if(ip_pktbuf == NULL) {
        _ERROR("%s: %m\n", "malloc failed!");
        return -1;
    }
    tourhdrp = EXTRACT_TOURHDRP(ip_pktbuf);

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
        _ERROR("%s: %m\n", "send_tourmsg()");
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
    struct sockaddr_in nextaddr;
    struct tourhdr *tourhdrp;

    memset(&nextaddr, 0, sizeof(nextaddr));
    /* point to the tourhdr inside the ip_pktbuf */
    tourhdrp = EXTRACT_TOURHDRP(ip_pktbuf);

    /* grab the next destination IP, and increment the index */
    nextaddr.sin_addr.s_addr = TOUR_NEXT(tourhdrp)->s_addr;
    tourhdrp->index = htons( (uint16_t)1 + ntohs(tourhdrp->index) );

    _DEBUG("Sending a tour msg to: %s of length %ld\n", getvmname(nextaddr.sin_addr), (u_long)ip_pktlen);

    craft_ip(ip_pktbuf, IPPROTO_TOUR, TOUR_IP_ID, host_ip, nextaddr.sin_addr, (ip_pktlen - IP4_HDRLEN));

    nextaddr.sin_family = AF_INET;
    nextaddr.sin_port = 0;
    n = sendto(rtsock, ip_pktbuf, ip_pktlen, 0, (struct sockaddr*)&nextaddr, sizeof(nextaddr));
    return n;
}

/**
* Monitors the rtsock and the multicast socket.
*
* ip_pktbuf of size ip_pktlen -- buffer for ip recving
* mcast_is_enabled -- 1 if we sent the initial tour msg, implying that
*                   our multicast socket has been set up.
*                   0 otherwise.
*
*/
int process_tour(struct ip *ip_pktbuf, size_t ip_pktlen, int mcast_is_enabled) {
    struct tourhdr *trhdrp;
    int erri, maxfpd1, is_first_tourmsg = 1, is_first_mcast = 1;
    ssize_t errs;
    socklen_t slen;
    struct sockaddr_in srcaddr;
    fd_set rset;
    struct timeval tval;
    struct timeval *tvalp = NULL;
    char *fmt = "<<<<< Node %s. I am a member of the group. >>>>>";
    char buf[BUFSIZE];

    tval.tv_sec = 5;
    tval.tv_usec = 0;
    FD_ZERO(&rset);
    maxfpd1 = MAX(rtsock, mcaster) + 1;
    for(EVER) {
        memset(&srcaddr, 0, sizeof(srcaddr));
        FD_SET(rtsock, &rset);
        FD_SET(mcaster, &rset);
        _DEBUG("%s\n", "Waiting to recv tour or multicast msgs...");
        erri = select(maxfpd1, &rset, NULL, NULL, tvalp);
        _DEBUG("select() returned: %d\n", erri);
        if(erri < 0) {
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        if(erri == 0) {
            /* Timeout was reached (tvalp changed when recv'ing mcast msgs)*/
            _INFO("%s", "Waited for all nodes to report. Ending the tour....\n");
            return 0;
        }
        if(FD_ISSET(rtsock, &rset)) {
            _DEBUG("%s\n", "Recv'ing on rtsock...");
            slen = sizeof(srcaddr);
            errs = recvfrom(rtsock, ip_pktbuf, ip_pktlen, 0, (struct sockaddr*)&srcaddr, &slen);
            if(errs < 0) {
                _ERROR("%s: %m\n", "recvfrom()");
                return -1;
            }
            if((validate_ip_tour(ip_pktbuf, (size_t)errs, &srcaddr)) < 0) {
                continue; /* ignore invalid packets */
            }
            trhdrp = EXTRACT_TOURHDRP(ip_pktbuf);
            /*print_tourhdr(trhdrp);*/
            if(is_first_tourmsg && !mcast_is_enabled) {
                /* only init the mulitcast socket once */
                erri = init_multicast_sock(trhdrp);
                if(erri < 0) {
                    return -1;
                }
            }
            is_first_tourmsg = 0;

            if(!is_pinging(srcaddr.sin_addr)) {
                /* start pinging prev ip*/
                erri = create_ping_sender(&srcaddr);
                if(erri < 0) {
                    return -1;
                }
            }

            if(TOUR_IS_OVER(trhdrp)) {
                erri = initiate_shutdown();
                if(erri < 0) {
                    return -1;
                }
                continue; /* we're done with this socket */
            }

            errs = send_tourmsg(ip_pktbuf, (size_t)errs);
            if(errs < 0) {
                _ERROR("%s: %m\n", "send_tourmsg()");
                return -1;
            }
            continue;
        }
        if(FD_ISSET(mcaster, &rset)) {
            _DEBUG("%s\n", "Recv'ing on multicast socket...");
            slen = sizeof(srcaddr);
            erri = recv_mcast_msg( &srcaddr, slen, buf, sizeof(buf));
            if(erri == -1) {
                return -1;
            } else if(erri == MSG_NOTVALID){
                continue;
            }
            buf[erri] = '\0';
            printf("Node %s. Received: %s\n", host_name, buf);

            if(is_first_mcast) {
                erri = stop_pinging(); /* immediately stop pinging */
                if(erri < 0) {
                    return -1;
                }

                erri = snprintf(buf,  sizeof(buf), fmt, host_name);
                if(erri < 0) {
                    _ERROR("%s: %m\n", "snprintf()");
                    return -1;
                }
                erri = send_mcast(buf, sizeof(buf));
                if(erri < 0) {
                    return -1;
                }
            }
            is_first_mcast = 0;
            tvalp = &tval; /* set the select's timeout! */

            continue;
        }
        return -1; /* should never be reached */
    }
    return 0;
}

/**
* We are the last stop on the route,initiate Tour shutdown.
* Waits 5 seconds then sends the shutdown multicast.
* RETURNS: 0 if ok, -1 if failure
*/
int initiate_shutdown(void) {
    int erri;
    struct timeval tm;
    char *fmt = "<<<<< This is node %s. Tour has ended. Group members please identify yourselves. >>>>>";
    char msg[BUFSIZE];
    tm.tv_sec = 5;
    tm.tv_usec = 0;

    _NOTE("%s\n", "Initiating tour shutdown.");

    erri = select(1, NULL, NULL, NULL, &tm); /* just for timeout*/
    if(erri != 0) {
        _ERROR("%s: %m\n", "select(for 5.2s timeout)");
        return -1;
    }
    erri = snprintf(msg, BUFSIZE, fmt, host_name);
    if(erri < 0) {
        _ERROR("%s: %m\n", "snprintf()");
        return -1;
    }

    return (send_mcast(msg, (size_t)erri));
}

int send_mcast(char *msg, size_t msglen) {
    int erri;
    printf("Node %s. Sending: %s\n", host_name, msg);

    erri = (int)sendto(mcaster, msg, msglen, 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));
    if(erri < 0) {
        _ERROR("%s: %m\n", "sendto(multicast)");
        return -1;
    }
    return 0;
}

/**
* Uses recvmsg() with IP_RECVORIGDSTADDR in order to check if the dst addr
* equals our multicast address.
* RETURNS: -1 if error, MSG_NOTVALID if msg invalid, or num bytes recv'd
*/
int recv_mcast_msg(struct sockaddr_in *srcaddr, socklen_t slen, char *buf, size_t buflen) {
    ssize_t errs;
    struct msghdr msg;
    struct iovec iov;
    struct cmsghdr *cmsgp = malloc(100);
    if(cmsgp == NULL) {
        _ERROR("%s: %m\n", "malloc()");
        return -1;
    }

    iov.iov_base = buf; /* buffer for recv'd UDP data */
    iov.iov_len = buflen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    msg.msg_name = srcaddr; /* the src of this UDP datagram */
    msg.msg_namelen = slen;

    msg.msg_control = cmsgp; /* for IP_RECVDSTADDR */
    msg.msg_controllen = 100;

    msg.msg_flags = 0;

    errs = recvmsg(mcaster, &msg, 0);
    if(errs < 0) {
        _ERROR("%s: %m\n", "recvmsg()");
        free(cmsgp);
        return -1;
    }
    if(msg.msg_controllen == 0) {
        _ERROR("%s", "Kernel didn't fill in control data, IP_RECVORIGDSTADDR didn't work?\n");
        free(cmsgp);
        exit(EXIT_FAILURE);/* fixme: remove*/
    }

    if((validate_mcast(&msg)) < 0) {
        free(cmsgp);
        return MSG_NOTVALID; /* ignore invalid packets */
    }

    free(cmsgp);
    return (int)errs;
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
    /* fixme: remove*/
    _INFO("multicast address is %s.\n", inet_ntoa(mcast_addr.sin_addr));
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
    memset(&greq, 0, sizeof(greq));
    greq.gr_interface = 0;
    memcpy(&greq.gr_group, &mcast_addr, sizeof(mcast_addr));
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

/**
* Prints recv'd timestamp if valid.
* RETURN: -1 if invalid or 0 if valid.
*/
int validate_ip_tour(struct ip *ip_pktp, size_t n, struct sockaddr_in *srcaddr) {
    struct tourhdr *trhdrp;
    struct in_addr prev_ip;
    time_t tm;
    if(n < TOURHDR_MIN) {
        _DEBUG("%s\n", "msg too small. Ignoring....");
        return -1;
    }
    if(ntohs(ip_pktp->ip_id) != TOUR_IP_ID) {
        _DEBUG("msg ip ID: %hhu, TOUR_IP_ID: %hhu. Don't match, ignoring....",
                ntohs(ip_pktp->ip_id), TOUR_IP_ID);
        return -1;
    }
    /* point past the ip header to the tour message */
    trhdrp = EXTRACT_TOURHDRP(ip_pktp);
    prev_ip = *(TOUR_PREV(trhdrp));
    if(prev_ip.s_addr != srcaddr->sin_addr.s_addr) {
        _DEBUG("%s\n", "Prev ip in msg != src ip of msg. Ignoring....");
        return -1;
    }
    if(TOUR_CURR(trhdrp)->s_addr != host_ip.s_addr) {
        _ERROR("CURR hostname from msg (%s) != ", getvmname(*TOUR_CURR(trhdrp)));
        printf("MY name (%s). Ignoring....\n", getvmname(host_ip));
        return -1;
    }
    tm = time(NULL);
    printf("%.24s received source routing packet from %s.\n", ctime(&tm), getvmname(prev_ip));

    return 0;
}

int validate_mcast(struct msghdr *msgp) {
    struct cmsghdr *cmsgp;
    struct in_addr *origdstaddr;
    for(cmsgp = CMSG_FIRSTHDR(msgp); cmsgp != NULL; cmsgp = CMSG_NXTHDR(msgp, cmsgp)) {
        _DEBUG("%s", "Looking at a cmsghdr.\n");
        if (cmsgp->cmsg_level != IPPROTO_IP || cmsgp->cmsg_type != IP_RECVORIGDSTADDR) {
            _DEBUG("%s\n", "cmsghdr not IPPROTO_IP or IP_RECVORIGDSTADDR. Ignoring....");
            continue;
        }
        origdstaddr = (struct in_addr *) CMSG_DATA(cmsgp);
        if (origdstaddr->s_addr == mcast_addr.sin_addr.s_addr) {
            _DEBUG("msg from CMSG_DATA multicast address was from %s.\n", inet_ntoa(*origdstaddr));
            return 0;
        }
        /** NOTE: for some reason the above doesn't work, even though it is
        * using the CMSG_DATA() system macro. My work around is the below.
        */
        origdstaddr = (struct in_addr*)(cmsgp+1) + 1;
        if (origdstaddr->s_addr == mcast_addr.sin_addr.s_addr) {
            _DEBUG("msg from MY multicast address was from %s.\n", inet_ntoa(*origdstaddr));
            return 0;
        }
        return -1;
    }
    return -1;
}


void print_tourhdr(struct tourhdr *trhdrp) {
#ifdef DEBUG
    struct in_addr *ipp = TOUR_FIRST(trhdrp);

    printf("=========== Tour Hdr ============\n");
    printf(" Mcast IP     : %16s\n", inet_ntoa(trhdrp->g_ip));
    printf(" Mcast port   : %16hu\n", ntohs(trhdrp->g_port));
    printf(" Index(hop)   : %16hu\n", ntohs(trhdrp->index));
    printf(" Num IPs(hops): %16hu\n", ntohl(trhdrp->num_ips));
    for(; ipp < (TOUR_FIRST(trhdrp) + 1 + ntohl(trhdrp->num_ips)); ipp++) {
        printf(" Host         : %16s", getvmname(*ipp));
        printf("%s", (ipp == TOUR_CURR(trhdrp)) ? " <--\n" : "\n");
    }
    printf("=================================\n");
#else
    trhdrp++; /* for -Wall -Wextra -Werror */
#endif
}

/**
* Create thread for ping replies and add it to the thread list.
*/
int create_ping_recver(void) {
    int erri;
    struct tident *tidentp;

    _NOTE("%s\n", "Creating the ping recver thread....");
    tidentp = malloc(sizeof(struct tident));
    if(tidentp == NULL) {
        _ERROR("%s: %m\n", "malloc()");
        return -1;
    }
    /* Create the thread for recving pings replies */
    erri = pthread_create(&tidentp->tid, NULL, &ping_recver, &pgrecver);
    if (0 > erri) {
        errno = erri;
        _ERROR("%s: %m\n", "pthread_create(ping_recver)");
        free(tidentp);
        return -1;
    }

    tidentp->dstaddr.s_addr = host_ip.s_addr;
    tidentp->is_sender = 0;
    LIST_INSERT_HEAD(&tidhead, tidentp, entries);

    return 0;
}

/**
* Create thread for ping requests and add it to the
* The argument to the ping_sender func is a {pgsender socket, {dstaddr}}
*/
int create_ping_sender(struct sockaddr_in *addr) {
    int erri;
    struct tident *tidentp;
    struct fd_addr *fd_addrp;

    _NOTE("%s\n", "Creating a ping sender thread....");
    tidentp = malloc(sizeof(struct tident));
    if(tidentp == NULL) {
        _ERROR("%s: %m\n", "malloc()");
        return -1;
    }
    /* create the arg for ping_sender */
    fd_addrp = malloc(sizeof(int) + sizeof(struct sockaddr_in));
    if(fd_addrp == NULL) {
        _ERROR("%s: %m\n", "malloc()");
        free(tidentp);
        return -1;
    }
    fd_addrp->sockfd = pgsender;
    memcpy(&fd_addrp->addr, addr, sizeof(fd_addrp->addr));

    /* the node we're pinging */
    tidentp->dstaddr.s_addr = addr->sin_addr.s_addr;
    tidentp->is_sender = 1;

    /* Create the thread for sending pings requests */
    erri = pthread_create(&tidentp->tid, NULL, &ping_sender, fd_addrp);
    if(erri > 0) {
        errno = erri;
        _ERROR("%s: %m\n", "pthread_create(ping_recver)");
        free(tidentp);
        free(fd_addrp);
        return -1;
    }

    LIST_INSERT_HEAD(&tidhead, tidentp, entries);

    return 0;
}

/**
* Stops all ping activity (sends and recvs).
* Cancel all threads. Join on all threads. Free the thread list.
*/
int stop_pinging(void) {
    struct tident *tidentp;
    void *retval;
    int erri;

    _NOTE("%s\n", "Cancelling all ping threads....");
    LIST_FOREACH(tidentp, &tidhead, entries) {
        erri = pthread_cancel(tidentp->tid);
        if(erri > 0)  {
            if(erri == ESRCH)
                continue;
            errno = erri;
            _ERROR("%s: %m\n", "pthread_cancel()");
            return -1;
        }
    }

    _NOTE("%s\n", "Joining all ping threads....");
    LIST_FOREACH(tidentp, &tidhead, entries) {
        erri = pthread_join(tidentp->tid, &retval);
        if(erri > 0) {
            errno = erri;
            _ERROR("%s: %m\n", "pthread_join()");
            return -1;
        }
        if(retval != PTHREAD_CANCELED) {
            _SPEC("%s\n", "A thread failed before being canceled.");
        }
    }

    /* free the whole list of thread ids */
    _DEBUG("%s\n", "Freeing the pthread id list....");
    while(tidhead.lh_first != NULL) {
        tidentp = tidhead.lh_first;
        if(tidentp->is_sender) {
            _DEBUG("I was pinging: %s\n", getvmname(tidentp->dstaddr));
        }
        LIST_REMOVE(tidentp, entries);
        free(tidentp);
    }
    return 0;
}

/**
* RETURNS: 1 if already pinging ipaddr, 0 otherwise
*/
int is_pinging(struct in_addr ipaddr) {
    struct tident *tidentp;

    _DEBUG("%s\n", "Searching if already prev node....");
    LIST_FOREACH(tidentp, &tidhead, entries) {
        if(tidentp->is_sender && tidentp->dstaddr.s_addr == ipaddr.s_addr) {
            return 1;
        }
    }
    return 0;
}
