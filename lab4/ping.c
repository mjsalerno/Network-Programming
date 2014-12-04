#include "ping.h"

extern struct in_addr host_ip;

/**
* This is a function for pthread_create()
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
void *ping_sender(void *pgsender_and_addr) {
    int pgsender;
    struct sockaddr_in dstaddr;
    struct hwaddr HWaddr;
    struct sockaddr_ll dstaddrll;
    socklen_t slen;
    int erri;
    ssize_t errs = 0;
    uint16_t icmpseq = 0;
    char ethbuf[ETH_FRAME_LEN];
    char pingdata[] = "ping";
    size_t ip_paylen;
    struct ip *ip_pktp = (struct ip*)(ethbuf + sizeof(struct ethhdr));
    struct icmp *icmpp = (struct icmp*)(ethbuf + sizeof(struct ethhdr) + sizeof(struct ip));

    /* copy out the socket fd and the dst addr from the malloc'd arg space */
    memcpy(&pgsender, pgsender_and_addr, sizeof(int));
    memcpy(&dstaddr, ((char*)pgsender_and_addr) + sizeof(int), sizeof(dstaddr));
    free(pgsender_and_addr);

    /*int pgrecver = *((int*)sock);*/
    /*char buf[sizeof(struct ip) + sizeof(struct icmp)];*/
    /*struct ip *iphdrp = (struct ip*)buf;*/
    /*struct icmp *icmphdrp = (struct icmp*)(buf + sizeof(struct ip));*/
    /*struct hwaddr dsthwaddr;*/

    printf("PING\n"); /* pg 748 */
    for(EVER) {
        unsigned char srcmac[6] = {0x00, 0x0c, 0x29, 0xe1, 0x54, 0xd1}; /* vm8: 00:0c:29:e1:54:d1 */
        slen = sizeof(dstaddr);
        erri = areq((struct sockaddr *) &dstaddr, slen, &HWaddr);
        if (erri < 0) {
            return NULL;
        }
        _DEBUG("%s", "Got a mac: ");
        print_hwa(HWaddr.sll_addr, HWaddr.sll_halen);
        printf("\n");

        craft_echo_reply(icmpp, icmpseq++, pingdata, sizeof(pingdata));
        ip_paylen = sizeof(struct icmp) + sizeof(pingdata);
        craft_ip(ip_pktp, IPPROTO_ICMP, PING_ICMP_ID, host_ip, dstaddr.sin_addr, ip_paylen);
        /*fixme: (u_char*)HWaddr.src_addr*/
        craft_eth(ethbuf, &dstaddrll, srcmac, (u_char*)HWaddr.sll_addr, HWaddr.sll_ifindex);

        errs = sendto(pgsender, ethbuf, (sizeof(struct ethhdr) + IP4_HDRLEN + ip_paylen), 0, (struct sockaddr*)&dstaddrll, sizeof(dstaddrll));
        if(errs < 0) {
            _ERROR("%s: %m\n", "sendto()");
            pthread_exit((void*)EXIT_FAILURE);
        }
    }
    /* todo: craft_ip(); send_on_iface(); */
    return NULL;
}

/**
* This is a function for pthread_create()
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
            _ERROR("%s: %m\n", "recvfrom()");
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
    if(icmpp->icmp_code != 0 || icmpp->icmp_type != ICMP_ECHOREPLY) {
        _DEBUG("%s\n", "icmp msg not an Echo reply. Ignoring....");
        return -1;
    }
    if(ntohs((uint16_t)icmpp->icmp_id) != PING_ICMP_ID) {
        _DEBUG("icmp Echo reply has wrong ID: %hu. Ignoring....\n", ntohs((uint16_t)icmpp->icmp_id));
        return -1;
    }
    return 0;
}

void craft_echo_reply(void *icmp_buf, uint16_t seq, void *data, size_t data_len) {

    struct icmp* icmp_pkt = icmp_buf;

    /* ICMP header */
    icmp_pkt->icmp_type = ICMP_ECHO;
    icmp_pkt->icmp_code = 0;
    icmp_pkt->icmp_id = htons(PING_ICMP_ID);
    icmp_pkt->icmp_seq = htons(seq);
    icmp_pkt->icmp_cksum = 0;
    memcpy(icmp_pkt + 1, data, data_len);
    icmp_pkt->icmp_cksum = csum(icmp_pkt, sizeof(struct icmp) + data_len);
}
