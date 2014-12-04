#include <assert.h>
#include "ping.h"
#include "ARP.h"
#include "tour.h"
#include "common.h"

void testrawip();
void test_csum ();
void test_add_arp();

struct in_addr host_ip;

int main() {
    //testrawip();
    test_csum();
    test_add_arp();
    return 0;
}

void test_csum() {

    //0x4500 0x003c 0x1c46 0x4000 0x4006 0xb1e6 0xac10 0x0a63 0xac10 0x0a0c -> 0xB1E6

    unsigned short fake_ip[10];

    fake_ip[0] = 0x4500;
    fake_ip[1] = 0x003c;
    fake_ip[2] = 0x1c46;
    fake_ip[3] = 0x4000;
    fake_ip[4] = 0x4006;
    fake_ip[5] = 0x0000; //csum set to zero
    fake_ip[6] = 0xac10;
    fake_ip[7] = 0x0a63;
    fake_ip[8] = 0xac10;
    fake_ip[9] = 0x0a0c;

    uint16_t mycsum = csum(fake_ip, 20);

    assert(mycsum == 0xB1E6);
}


void testrawip() {
    int rtsock;
    const int on = 1;
    ssize_t n;
    struct ip *ip_pktp;
    struct sockaddr_in addr;
    socklen_t slen;
    char buf[IP_MAXPACKET];
    ip_pktp = (struct ip *) (buf);

    rtsock = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR);
    if (rtsock < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        exit(EXIT_FAILURE);
    }
    if ((setsockopt(rtsock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on))) < 0) {
        _ERROR("%s: %m\n", "setsockopt(IP_HDRINCL)");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    printf("Dst/src: %s , %s\n", getvmname(addr.sin_addr), inet_ntoa(addr.sin_addr));

    craft_ip(ip_pktp, IPPROTO_TOUR, TOUR_IP_ID, addr.sin_addr, addr.sin_addr, 0);

    _DEBUG("%s\n", "Sending...");
    n = sendto(rtsock, ip_pktp, IP4_HDRLEN, 0, (struct sockaddr *) &addr, sizeof(addr));
    if (n < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "Waiting to recv...");
    slen = sizeof(addr);
    n = recvfrom(rtsock, ip_pktp, IP_MAXPACKET, 0, (struct sockaddr *) &addr, &slen);
    if (n < 0) {
        _ERROR("%s: %m\n", "socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)");
        exit(EXIT_FAILURE);
    }
}
void test_add_arp() {
    int err;
    struct arp_cache* arp_head = NULL;
    struct in_addr sin_addr;

    unsigned char mac1[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};

    err = inet_aton("123.234.213.9", &sin_addr);
    assert(err != 0);

    assert(arp_head == NULL);
    add_arp(&arp_head, sin_addr.s_addr, 1, 0xF, 6, mac1);
    assert(arp_head != NULL);
    assert(arp_head->next == NULL);

    add_arp(&arp_head, sin_addr.s_addr, 1, 0xF, 6, mac1);
    assert(arp_head->next->next == NULL);
}
