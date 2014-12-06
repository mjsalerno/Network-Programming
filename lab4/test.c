#include <assert.h>
#include "ping.h"
#include "ARP.h"
#include "tour.h"
#include "common.h"

void testrawip();
void test_csum ();
void test_add_arp();
void test_CMSG_MACROS();

struct in_addr host_ip;
static struct sockaddr_in mcast_addr;
static int mcaster;

int main() {
    //testrawip();
    test_csum();
    test_add_arp();
    test_CMSG_MACROS();
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
    struct in_addr sin_addr1;
    struct in_addr sin_addr2;
    struct hwa_ip hwaip;
    struct arp_cache* a;
    struct arp_cache* b;

    unsigned char mac1[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00};

    err = inet_aton("123.234.213.9", &sin_addr1);
    assert(err != 0);

    assert(arp_head == NULL);
    a = add_arp(&arp_head, sin_addr1.s_addr, 1, 0XF, 6, mac1, &hwaip, 0);
    assert(arp_head != NULL);
    assert(arp_head->next == NULL);

    err = inet_aton("223.234.213.9", &sin_addr2);
    assert(err != 0);

    b = add_part_arp(&arp_head, sin_addr2.s_addr, -1);
    assert(arp_head->next->next == NULL);

    assert(a == get_arp(arp_head, sin_addr1.s_addr));
    assert(b == get_arp(arp_head, sin_addr2.s_addr));

    free_arp_cache(&arp_head);
    assert(arp_head == NULL);
    free_arp_cache(NULL);
}

int validate_mcast(struct msghdr *msgp) {
    struct cmsghdr *cmsgp;
    struct in_addr *origdstaddr;
    cmsgp = msgp->msg_control;

    /* BE CAREFUL: CMSG_XXXX macros *may* be broke on 64-bit systems. */
    /*for(cmsgp = CMSG_FIRSTHDR(msgp); cmsgp != NULL; cmsgp = CMSG_NXTHDR(msgp, cmsgp)) {*/
    printf("%s", "Looking at a cmsghdr.\n");
    if (cmsgp->cmsg_level != IPPROTO_IP || cmsgp->cmsg_type != IP_RECVORIGDSTADDR) {
        printf("%s\n", "cmsghdr not IPPROTO_IP or IP_RECVORIGDSTADDR. Ignoring....");
        /*continue;*/
        return -1;
    }
    origdstaddr = (struct in_addr *) CMSG_DATA(cmsgp);
    if (origdstaddr->s_addr == mcast_addr.sin_addr.s_addr) {
        printf("msg from CMSG_DATA multicast address was from %s.\n", inet_ntoa(*origdstaddr));
        return 0;
    }
    /** NOTE: for some reason the above doesn't work, even though it is
    * using the CMSG_DATA() system macro. My work around is the below.
    */
    origdstaddr = (struct in_addr*)(cmsgp+1) + 1;
    if (origdstaddr->s_addr == mcast_addr.sin_addr.s_addr) {
        printf("msg from MY multicast address was from %s.\n", inet_ntoa(*origdstaddr));
        return 0;
    }
    return -1;
    /*}
    return -1;*/
}

int recv_mcast_msg(struct sockaddr_in *srcaddr, socklen_t slen, char *buf, size_t buflen) {
    ssize_t errs;
    struct msghdr msg;
    struct iovec iov;
    struct cmsghdr *cmsgp = malloc(100);
    if(cmsgp == NULL) {
        printf("%s: %m\n", "malloc()");
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

    printf("%s\n", "Waiting for msg...");
    errs = recvmsg(mcaster, &msg, 0);
    if(errs < 0) {
        printf("%s: %m\n", "recvmsg()");
        free(cmsgp);
        return -1;
    }
    printf("control data len: %lu\n", (u_long)msg.msg_controllen);
    if(msg.msg_controllen < sizeof(struct cmsghdr)) {
        printf("%s", "Kernel didn't fill in control data, IP_RECVORIGDSTADDR didn't work?\n");
        free(cmsgp);
        return -1;
    }

    if((validate_mcast(&msg)) < 0) {
        free(cmsgp);
        return -2; /* ignore invalid packets */
    }

    free(cmsgp);
    return (int)errs;
}

void test_CMSG_MACROS() {
    const int on = 1, ttl = 1;
    int erri;
    struct sockaddr_in bindaddr;
    struct sockaddr_in srcaddr;
    struct group_req greq;
    char buf[1600];
    ssize_t errs;

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    memset(&srcaddr, 0, sizeof(srcaddr));

    mcaster = socket(AF_INET, SOCK_DGRAM, 0);
    if(mcaster < 0) {
        printf("%s: %m\n", "socket(AF_INET, SOCK_DGRAM, 0)");
        exit(EXIT_FAILURE);
    }
    /* we want to use recvmsg() to get the orig dst addr of multicasts */
    erri = setsockopt(mcaster, IPPROTO_IP, IP_RECVORIGDSTADDR, &on, sizeof(on));
    if(erri < 0) {
        printf("%s: %m\n", "setsockopt(IP_RECVORIGDSTADDR)");
        close(mcaster);
        exit(EXIT_FAILURE);
    }

    memset(&mcast_addr, 0, sizeof(mcast_addr));
    mcast_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "235.235.235.235", &mcast_addr.sin_addr);
    mcast_addr.sin_port = htons(55555);
    printf("multicast address is %s.\n", inet_ntoa(mcast_addr.sin_addr));

    /* bind to INADDR_ANY, fine as long as we validate incoming pkts */
    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = mcast_addr.sin_port;
    printf("%s\n", "Binding mcast socket...");
    erri = bind(mcaster, (struct sockaddr*)&bindaddr, sizeof(bindaddr));
    if(erri < 0) {
        _ERROR("%s: %m\n", "bind()");
        exit(EXIT_FAILURE);
    }

    /* let the kernel pick the interface index */
    memset(&greq, 0, sizeof(greq));
    greq.gr_interface = 0;
    memcpy(&greq.gr_group, &mcast_addr, sizeof(mcast_addr));
    printf("%s\n", "Joining the mcast group...");
    erri = setsockopt(mcaster, IPPROTO_IP, MCAST_JOIN_GROUP, &greq, sizeof(greq));
    if(erri < 0) {
        printf("%s: %m\n", "setsockopt(MCAST_JOIN_GROUP)");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", "Setting the mcast TTL to 1...");
    erri = setsockopt(mcaster, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    if(erri < 0) {
        printf("%s: %m\n", "setsockopt(IP_MULTICAST_TTL)");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", "Enabling loopback...");
    erri = setsockopt(mcaster, IPPROTO_IP, IP_MULTICAST_LOOP, &on, sizeof(on));
    if(erri < 0) {
        printf("%s: %m\n", "setsockopt(IP_MULTICAST_LOOP)");
        exit(EXIT_FAILURE);
    }


    printf("%s\n", "Sending mcast socket...");
    errs = sendto(mcaster, "I'm a multicast.", 17, 0, (struct sockaddr*)&mcast_addr, sizeof(mcast_addr));
    if(errs < 0) {
        printf("%s: %m\n", "sendto(mcaster)");
        exit(EXIT_FAILURE);
    }

    recv_mcast_msg(&srcaddr, sizeof(srcaddr), buf, sizeof(buf));
}
