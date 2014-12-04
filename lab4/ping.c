#include "ping.h"

/**
* The argument sock is a int* to a sockaddr_in.
* FIXME: arg has to be void* to {pgsender socket, {dstaddr}}, free it too.
*
* When a node is ready to start pinging, it first prints out a ‘PING’
* message similar to lines 32-33 of Figure 28.5,  p. 744. It then builds
* up ICMP echo request messages and sends them to the source node every 1
* second through the PF_PACKET socket. It also reads incoming echo response
* messages off the pg socket, in response to which it prints out the same kind
* of output as the code of Figure 28.8,  p. 748.
*/
void *ping_sender(void /* *addr */) {
    struct hwaddr HWaddr;
    struct sockaddr_in dstaddr;
    socklen_t slen;
    int erri;
    /*int pgrecver = *((int*)sock);*/
    /*char buf[sizeof(struct ip) + sizeof(struct icmp)];*/
    /*struct ip *iphdrp = (struct ip*)buf;*/
    /*struct icmp *icmphdrp = (struct icmp*)(buf + sizeof(struct ip));*/
    /*struct hwaddr dsthwaddr;*/
    /*struct sockaddr_ll dstaddr;*/

    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = INADDR_LOOPBACK;
    slen = sizeof(dstaddr);

    erri = areq((struct sockaddr*)&dstaddr, slen, &HWaddr);
    if(erri < 0) {
        return NULL;
    }
    _DEBUG("%s", "Got a mac: ");
    print_hwa(HWaddr.sll_addr, HWaddr.sll_halen);
    printf("\n");

    /* todo: craft_ip(); send_on_iface(); */
    return NULL;
}

/**
* sock: actually int* pgrecver, don;t have to free pgrecverp
*       This is a AF_INET, raw socket.
*
*/
void *ping_recver(void *pgrecverp) {
    /**/
    int pgrecver = *(int*)pgrecverp;
    struct sockaddr_in srcaddr;
    socklen_t slen;
    ssize_t errs;
    char buf[IP_MAXPACKET];
    struct ip *ip_pktp = (struct ip*)buf;
    struct icmp *icmpp;


    for(EVER) {
        slen = sizeof(srcaddr);
        memset(&srcaddr, 0, slen);
        errs = recvfrom(pgrecver, ip_pktp, IP_MAXPACKET, 0, (struct sockaddr*)&srcaddr, &slen);
        if(errs < 0) {
            _ERROR("%s: %m\n", "ping_recver()");
            pthread_exit((void*)EXIT_FAILURE);
        }
        if((filter_ip_icmp(ip_pktp, (size_t)errs)) < 0 ) {
            continue;
        }
        icmpp = EXTRACT_ICMPHDRP(ip_pktp);
        printf("%d bytes from %s: seq=%u, ttl=%d\n", (int)errs,
                getvmname(srcaddr.sin_addr), icmpp->icmp_seq, ip_pktp->ip_ttl);

    }

    pthread_exit((void*)EXIT_SUCCESS);
}

int filter_ip_icmp(struct ip *ip_pktp, size_t n) {
    struct icmp *icmpp;

    if(ip_pktp->ip_p != IPPROTO_ICMP) {
        _DEBUG("%s\n", "ip proto not IPPROTO_ICMP. Ignoring....");
        return -1;
    }
    if(n < (IP4_HDRLEN + ICMP_HDRLEN)) {
        _DEBUG("%s\n", "icmp msg too small. Ignoring....");
        return -1;
    }
    if(ntohs(ip_pktp->ip_id) != PING_ICMP_ID) {
        _DEBUG("msg ip ID: %hhu, PING_ICMP_ID: %hhu. Don't match, ignoring....",
                ntohs(ip_pktp->ip_id), PING_ICMP_ID);
        return -1;
    }
    /* point past the ip header to the icmp header */
    icmpp = EXTRACT_ICMPHDRP(ip_pktp);
    icmpp++;
    return 0;
}
