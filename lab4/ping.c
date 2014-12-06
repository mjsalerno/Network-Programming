#include "ping.h"

extern struct in_addr host_ip;

/**
* This is a function for pthread_create()
* The argument is a struct fd_addr.
*/
void *ping_sender(void *fd_addrp) {
    int pgsender;
    struct sockaddr_in dstaddr;
    struct fd_addr *fdaddr = (struct fd_addr*)fd_addrp;
    struct hwaddr HWaddr;
    struct sockaddr_ll dstaddrll;
    socklen_t slen;
    int erri;
    ssize_t errs = 0;
    uint16_t icmpseq = 0;
    char ethbuf[ETH_FRAME_LEN];
    char pingdata[] = "533 Grading TAs are cool!";
    size_t ip_paylen;
    struct ip *ip_pktp = (struct ip*)(ethbuf + sizeof(struct ethhdr));
    struct icmp *icmpp = (struct icmp*)(ethbuf + sizeof(struct ethhdr) + IP4_HDRLEN);

    memset(ethbuf, 0, sizeof(ethbuf));

    /* copy out the socket fd and the dst addr from the malloc'd arg space */
    pgsender = fdaddr->sockfd;
    memcpy(&dstaddr, &fdaddr->addr, sizeof(dstaddr));
    free(fdaddr); /* the argument was malloc'd */

    printf("PING %s (%s): %u bytes of data.\n", getvmname(dstaddr.sin_addr),
            inet_ntoa(dstaddr.sin_addr), (u_int)sizeof(pingdata));
    for(EVER) {
        slen = sizeof(dstaddr);
        erri = areq((struct sockaddr *) &dstaddr, slen, &HWaddr);
        if (erri < 0) {
            pthread_exit((void*)EXIT_FAILURE);
        }

        craft_echo_reply(icmpp, icmpseq++, pingdata, sizeof(pingdata));
        ip_paylen = ICMP_MINLEN + sizeof(pingdata);
        craft_ip(ip_pktp, IPPROTO_ICMP, PING_ICMP_ID, host_ip, dstaddr.sin_addr, ip_paylen);
        craft_eth(ethbuf, ETH_P_IP, &dstaddrll, HWaddr.src_addr, HWaddr.dst_addr, HWaddr.src_ifindex);

        _DEBUGY("Sending an icmp ping, ip_paylen: %u, total len: %u\n", (u_int)ip_paylen, (u_int)(sizeof(struct ethhdr) + IP4_HDRLEN + ip_paylen));
        errs = sendto(pgsender, ethbuf, (sizeof(struct ethhdr) + IP4_HDRLEN + ip_paylen), 0, (struct sockaddr*)&dstaddrll, sizeof(dstaddrll));
        if(errs < 0) {
            _ERROR("%s: %m\n", "sendto()");
            pthread_exit((void*)EXIT_FAILURE);
        }
        sleep(1); /* sleep for a second  before sending another ping */
    }
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
        buf[errs] = 0;
        _DEBUGY("%s\n", "Got stuff from ping recv'er....");
        if((filter_ip_icmp(ip_pktp, (size_t)errs)) < 0 ) {
            continue;
        }
        icmpp = EXTRACT_ICMPHDRP(ip_pktp);
        printf("%d bytes from %s: id=0x%x seq=%hu, ttl=%d, data: %s\n", (int)errs,
                getvmname(srcaddr.sin_addr), ntohs(icmpp->icmp_id), ntohs(icmpp->icmp_seq),
                ip_pktp->ip_ttl, (((char*)icmpp) + ICMP_MINLEN));
    }
}

int filter_ip_icmp(struct ip *ip_pktp, size_t n) {
    struct icmp *icmpp;

    if(ip_pktp->ip_p != IPPROTO_ICMP) {
        _DEBUG("ip proto: 0x%x is not IPPROTO_ICMP. Ignoring....\n", ip_pktp->ip_p);
        return -1;
    }
    if(n < (IP4_HDRLEN + ICMP_MINLEN)) {
        _DEBUG("icmp msg too small, len: %u bytes. Ignoring....\n", (u_int)n);
        return -1;
    }
    _DEBUG("msg IP.id: 0x%x, PING_ICMP_ID: 0x%x\n", ntohs(ip_pktp->ip_id), PING_ICMP_ID);

    /* point past the ip header to the icmp header */
    icmpp = EXTRACT_ICMPHDRP(ip_pktp);
    if(icmpp->icmp_code != 0 || icmpp->icmp_type != ICMP_ECHOREPLY) {
        _DEBUG("icmp msg not echo reply, type 0x%x, code: 0x%x. Ignoring....\n", icmpp->icmp_type, icmpp->icmp_code);
        return -1;
    }
    if(ntohs((uint16_t)icmpp->icmp_id) != PING_ICMP_ID) {
        _DEBUG("icmp echo reply has wrong ID: 0x%x. Ignoring....\n", ntohs((uint16_t)icmpp->icmp_id));
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
    memcpy(icmp_buf + ICMP_MINLEN, data, data_len);
    icmp_pkt->icmp_cksum = csum(icmp_pkt, ICMP_MINLEN + data_len);
}
