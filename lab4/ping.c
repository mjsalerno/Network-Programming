#include "ping.h"
#include "tour.h"

/**
* The argument sock is a int* to a socket.
*/
void *ping(void /* *sock */) {
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