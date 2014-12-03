#include "ping.h"

/**
* The argument sock is a int* to a socket.
*
* When a node is ready to start pinging, it first prints out a ‘PING’
* message similar to lines 32-33 of Figure 28.5,  p. 744. It then builds
* up ICMP echo request messages and sends them to the source node every 1
* second through the PF_PACKET socket. It also reads incoming echo response
* messages off the pg socket, in response to which it prints out the same kind
* of output as the code of Figure 28.8,  p. 748.
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