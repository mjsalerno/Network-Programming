#include "tour.h"

/* prototypes for private funcs */
int socket_ip_raw(int proto);
int init_tour_msg(void *hdrbuf, char *vms[], int n);
void ping(void *null);


int pgsock;
char host_name[128];
struct in_addr host_ip;

int main(int argc, char *argv[]) {
    int rtsock;
    int erri, startedtour = 0;
    char *hdrbuf = NULL;
    struct hostent *he;

    erri = gethostname(host_name, 128); /* get the host name */
    if (erri < 0) {
        perror("ERROR gethostname()");
        exit(EXIT_FAILURE);
    }
    he = gethostbyname(host_name);      /* get the host ip */
    if (he == NULL) {
        herror("ERROR: gethostbyname()");
        exit(EXIT_FAILURE);
    }
    /* take out the first addr from the h_addr_list */
    host_ip = **((struct in_addr **) (he->h_addr_list));


    if (argc > 1) {
        startedtour = 1;
    }

    rtsock = socket_ip_raw(IPPROTO_TOUR);
    pgsock = socket_ip_raw(IPPROTO_ICMP); /* make the ping pg socket for the threads */

    if (startedtour) {               /* We are initiating a new tour. */
        /* We'll have (argc - 1) stops in the tour */
        hdrbuf = malloc(sizeof(struct tourhdr) +
                sizeof(struct in_addr) * (argc - 1));
        erri = init_tour_msg(hdrbuf, (argv + 1), (argc - 1));
        if (erri < 0) {
            perror("ERROR: init_tour_msg()");
            goto cleanup;
        }
    }


    // listen for tour msgs on rt socket, ping on pg socket

    if (startedtour)
        free(hdrbuf);
    close(rtsock);
    close(pgsock);
    exit(EXIT_SUCCESS);
    cleanup:
    if (startedtour)
        free(hdrbuf);
    close(rtsock);
    close(pgsock);
    exit(EXIT_FAILURE);
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

int socket_ip_raw(int proto) {
    int fd;
    fd = socket(AF_INET, SOCK_RAW, proto);
    if(fd < 0) {
        _ERROR("socket(AF_INET, SOCK_RAW, %d): %m\n", proto);
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
    hdrp->g_ip.s_addr = 0; /* fixme */
    hdrp->g_port = 0; /* fixme */
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
