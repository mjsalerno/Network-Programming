#include "tour.h"

/* prototypes for private funcs */
int start_tour(int argc, char *argv[]);
int handle_tour(void);

/* sockets */
static int pgrecver; /* PF_INET RAW -- receiving pings replies */
static int pgsender; /* PF_PACKET RAW -- sending ping echo requests */
static int rtsock; /* PF_INET RAW HDR_INCLD -- recv/sending tour messages */

static char host_name[128];
static struct in_addr host_ip;

/**
* Sets up the sockets then calls start_tour() and handle_tour()
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
        exit(EXIT_FAILURE);
    }
    pgrecver = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR);
    if(pgrecver < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        exit(EXIT_FAILURE);
    }

    erri = start_tour(argc, argv); /* start tour if needed */
    if(erri < 0) {
        goto cleanup;
    }

    erri = handle_tour(); /* now handle all messages */
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


/**
* If the process was invoked with a list of hostnames then it will send the
* initial tour msg. Otherwise it will just return.
*/
int start_tour(int argc, char *argv[]) {
    char *hdrbuf = NULL;
    int erri;

    if(argc <= 1) {
        return 0;
    }
    /* Else we are initiating a new tour. */

    /* We'll have (argc - 1) nodes in the tour */
    hdrbuf = malloc(sizeof(struct tourhdr) + sizeof(struct in_addr) * (argc - 1));
    erri = init_tour_msg(hdrbuf, (argv + 1), (argc - 1));
    if (erri < 0) {
        perror("ERROR: init_tour_msg()");
        free(hdrbuf);
        return -1;
    }

    /* fixme: send the initial tour msg here. */

    free(hdrbuf);
    return 0;
}

/**
* Handles all of the tour processing (except for sending the initial msg).
*/
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
