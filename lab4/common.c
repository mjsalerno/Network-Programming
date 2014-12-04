#include <stdio.h>
#include <net/if.h>
#include "common.h"
#include "tour.h"

void craft_eth(void* eth_buf, struct sockaddr_ll* raw_addr, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], int ifindex) {
    struct ethhdr* et = eth_buf;
    if(raw_addr != NULL) {
        /*prepare sockaddr_ll*/
        memset(raw_addr, 0, sizeof(struct sockaddr_ll));

        /*RAW communication*/
        raw_addr->sll_family = PF_PACKET;
        raw_addr->sll_protocol = htons(ARP_ETH_PROTO);

        /*index of the network device*/
        raw_addr->sll_ifindex = ifindex;

        /*ARP hardware identifier is ethernet*/
        raw_addr->sll_hatype = 0;
        raw_addr->sll_pkttype = 0;

        /*address length*/
        raw_addr->sll_halen = ETH_ALEN;
        /* copy in the dest mac */
        memcpy(raw_addr->sll_addr, dst_mac, ETH_ALEN);
        raw_addr->sll_addr[6] = 0;
        raw_addr->sll_addr[7] = 0;
    }

    et->h_proto = htons(ARP_ETH_PROTO);
    memcpy(et->h_dest, dst_mac, ETH_ALEN);
    memcpy(et->h_source, src_mac, ETH_ALEN);

    printf("dst mac: ");
    print_hwa(et->h_dest, ETH_ALEN);
    printf("\nsrc mac: ");
    print_hwa(et->h_source, ETH_ALEN);
    printf("\n");

    _DEBUG("crafted frame with proto: %d\n", et->h_proto);
}

void craft_ip(void* ip_pktbuf, uint8_t proto, u_short ip_id, struct in_addr src_ip, struct in_addr dst_ip, size_t paylen) {
    struct ip* ip_pkt = ip_pktbuf;
    if(ip_pktbuf == NULL) {
        _ERROR("%s\n", "ip_pktbuf is NULL!");
        exit(EXIT_FAILURE);
    }

    /*IPv4 header*/
    ip_pkt->ip_hl = IP4_HDRLEN / sizeof (uint32_t);
    ip_pkt->ip_v = IPVERSION;
    ip_pkt->ip_tos = 0;
    ip_pkt->ip_len = htons((uint16_t)(IP4_HDRLEN + paylen));
    ip_pkt->ip_id = htons(ip_id);
    ip_pkt->ip_off = htons(IP_DF);
    ip_pkt->ip_ttl = 25;
    ip_pkt->ip_p = proto;
    ip_pkt->ip_dst.s_addr = dst_ip.s_addr;
    ip_pkt->ip_src.s_addr = src_ip.s_addr;
    /*memcpy(&(ip_pkt->ip_dst.s_addr), &dst_ip.s_addr, sizeof(in_addr_t));
    memcpy(&(ip_pkt->ip_src.s_addr), &src_ip.s_addr, sizeof(in_addr_t));*/
    ip_pkt->ip_sum = 0;
}

/**
* makes an arp packet, you need to malloc it and make sure there is enough room
* for the ip addresses and stuff
*/
size_t craft_arp(struct arphdr* arp, unsigned short int ar_op,  unsigned short int ar_pro, unsigned short int ar_hrd, unsigned char ar_sha[ETH_ALEN], unsigned char ar_sip[4], unsigned char ar_tha[ETH_ALEN], unsigned char ar_tip[4]) {
    char* ptr;
    size_t add_len = 0;

    /*unsigned short int ar_hrd;	Format of hardware address.
    unsigned short int ar_pro;		 Format of protocol address.
    unsigned short int ar_op;		 ARP opcode (command).  */

    arp->ar_pln = sizeof(uint32_t);
    arp->ar_hrd = ar_hrd; /*ARPHRD_ETHER*/
    arp->ar_pro = ar_pro; /*ETHERTYPE_IP*/
    arp->ar_op = ar_op;

    if(ar_hrd == ARPHRD_ETHER) {
        arp->ar_hln = ETH_ALEN;
    } else {
        _ERROR("Do not know ar_hrd: %d\n", ar_hrd);
        exit(EXIT_FAILURE);
    }

    if(ar_pro == ETHERTYPE_IP) {
        add_len = sizeof(uint32_t);
    } else {
        _ERROR("Do not know ar_pro: %d\n", ar_pro);
        exit(EXIT_FAILURE);
    }

    /*unsigned char __ar_sha[ETH_ALEN];	 Sender hardware address.
    unsigned char __ar_sip[4];		 Sender IP address.
    unsigned char __ar_tha[ETH_ALEN];	 Target hardware address.
    unsigned char __ar_tip[4];		 Target IP address.*/
    ptr = (char*)(arp+1);
    memcpy(ptr, ar_sha, arp->ar_hln);
    ptr += arp->ar_hln;
    memcpy(ptr, ar_sip, add_len);
    ptr += add_len;
    if(ar_op != ARPOP_REQUEST)
        memcpy(ptr, ar_tha, arp->ar_hln);
    ptr += add_len;
    memcpy(ptr, ar_tip, add_len);
    ptr += add_len;

    _DEBUG("crafted an arp pkt of size: %d\n", (int)((long)ptr - (long)arp));

    return (size_t)((long)ptr - (long)arp);

}

unsigned char* extract_target_addy(struct arphdr* arp) {

    unsigned char* ptr = (unsigned char*)arp+1;
    size_t add_len = 0;

    if(arp->ar_pro == ETHERTYPE_IP && arp->ar_op == ARPOP_REQUEST) {
        add_len = sizeof(uint32_t);
        ptr += arp->ar_hln;
        ptr += add_len;
        ptr += add_len;
    } else {
        _ERROR("only know ar_pro %d, not: %d\n", ETHERTYPE_IP, arp->ar_pro);
        _ERROR("can't get target ip if not a request, want: %d, got: %d\n", ARPOP_REQUEST, arp->ar_op);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void craft_icmp(void* icmp_buf, void* data, size_t data_len) {

    struct icmp* icmp_pkt = icmp_buf;

    /* ICMP header */
    icmp_pkt->icmp_type = ICMP_ECHO;
    icmp_pkt->icmp_code = 0;
    icmp_pkt->icmp_id = htons(1000);
    icmp_pkt->icmp_seq = htons(0);
    icmp_pkt->icmp_cksum = 0;
    memcpy(icmp_pkt + 1, data, data_len);
    icmp_pkt->icmp_cksum = csum(icmp_pkt, sizeof(struct icmp) + data_len);
}

/*calculates the checksum everything*/
uint16_t csum(void*data, size_t len) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*) data;

    /*subtract two each time since we are going by bytes*/
    for(; len > 1; ptr += 1) {
        sum += *ptr;
        printf("new sum: %X\n", sum);

        /*if sums most sig bit set then wrap around, cuz 1's comp addition*/
        if(sum & 0x10000) {
            sum = (sum & 0xFFFF) + ((sum >> 16) & 0xFFFF);
            printf("sum after wrap: %X\n", sum);
        }

        /*dont update the len in forloop so we get an extra loop*/
        len -= 2;
    }

    /*if there was not an even amount of bytes*/
    if(len > 0) {
        sum += *ptr;
    }

    /*fold 32 bits into 16*/
    if(((sum >> 16) & 0xFFFF) > 0) {
        sum = (sum & 0xFFFF) + ((sum >> 16) & 0xFFFF);
    }

    /*if there is stuff in the high bits, I probably did it wrong*/
    if(((sum >> 16) & 0xFFFF) > 0) {
        _ERROR("%s\n", "yew sux, try again");
        exit(EXIT_FAILURE);
    }

    /*take ones comp and return it*/
    printf("rtn sum before not: %X\n", sum);
    return ((uint16_t) ~sum);
}

/**
* Write "n" bytes to a descriptor.
* RETURN: "n", the of bytes written or -1 on error
**/
ssize_t write_n(int fd, void *buf, size_t n) {
    ssize_t curr_n = 0;
    size_t tot_n = 0;

    while(0 < n){
        curr_n = write(fd, buf, n);
        if(curr_n <= 0){
            if(errno == EINTR){
                continue;
            }
            return -1;
        }
        else{
            n -= curr_n;
            tot_n += curr_n;
            buf = ((char*)buf) + curr_n;
        }
    }
    return tot_n;
}

void print_hwa(unsigned char* mac, char mac_len) {
    int n;
    char *fmt_first = "%02x", *fmt_rest = ":%02x";
    char *fmt = fmt_first;

    for(n = 0; n < mac_len; n++) {
        printf(fmt, mac[n]);
        fmt = fmt_rest;
    }
}

char *getvmname(struct in_addr vmaddr) {
    struct hostent *he;
    char *name;
    /*int i = 0;*/
    if(NULL == (he = gethostbyaddr(&vmaddr, 4, AF_INET))) {
        herror("ERROR gethostbyaddr()");
        _ERROR("Target ip for lookup was: %s\n", inet_ntoa(vmaddr));
        exit(EXIT_FAILURE);
    }
    name = he->h_name;
    /*while(name != NULL && name[0] != 'v' && name[1] != 'm') {
        name = he->h_aliases[i];
        i++;
    }*/
    return name;
}

/* Fill in the hostname and the host ip. Calls gethostname , gethostbyname */
int gethostname_ip(char *host_name, struct in_addr *host_ip) {
    int erri;
    struct hostent *he;
    erri = gethostname(host_name, 128); /* get the host name */
    if (erri < 0) {
        perror("ERROR gethostname()");
        return -1;
    }
    he = gethostbyname(host_name);      /* get the host ip */
    if (he == NULL) {
        herror("ERROR: gethostbyname()");
        return -1;
    }
    /* take out the first addr from the h_addr_list */
    *host_ip = **((struct in_addr **) (he->h_addr_list));

    return 0;
}

