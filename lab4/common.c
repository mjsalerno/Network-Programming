#include <stdio.h>
#include <net/if.h>
#include "common.h"

size_t craft_eth(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len) {
    struct ethhdr* et = buff;
    if(data_len > ETH_DATA_LEN) {
        _ERROR("%s\n", "ERROR: craft_frame(): data_len too big");
        exit(EXIT_FAILURE);
    }

    if(raw_addr != NULL) {
        /*prepare sockaddr_ll*/
        memset(raw_addr, 0, sizeof(struct sockaddr_ll));

        /*RAW communication*/
        raw_addr->sll_family = PF_PACKET;
        raw_addr->sll_protocol = htons(PROTO);

        /*index of the network device*/
        raw_addr->sll_ifindex = index;

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

    et->h_proto = htons(PROTO);
    memcpy(et->h_dest, dst_mac, ETH_ALEN);
    memcpy(et->h_source, src_mac, ETH_ALEN);

    /* copy in the data and ethhdr */
    memcpy(buff + sizeof(struct ethhdr), data, data_len);

    _DEBUG("crafted frame with proto: %d\n", et->h_proto);
    return sizeof(struct ethhdr) + data_len;
}

size_t craft_ip(int index, struct sockaddr_ll* raw_addr, void* buff, in_addr_t src_ip, in_addr_t dst_ip, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len) {
    struct ip* ip_pkt = malloc(sizeof(struct ip) + data_len);
    if(ip_pkt == NULL) {
        _ERROR("%s\n", "malloc failed makeing the ip_pkt");
        exit(EXIT_FAILURE);
    }

    /*IPv4 header*/
    ip_pkt->ip_hl = IP4_HDRLEN / sizeof (uint32_t);
    ip_pkt->ip_v = 4;
    ip_pkt->ip_tos = 0;
    ip_pkt->ip_len = htons((uint16_t)(IP4_HDRLEN + ICMP_HDRLEN + data_len));
    ip_pkt->ip_id = htons(0);
    ip_pkt->ip_off = IP_DF;
    ip_pkt->ip_ttl = 255;
    ip_pkt->ip_p = IPPROTO_ICMP;
    memcpy(&(ip_pkt->ip_dst.s_addr), &dst_ip, sizeof(in_addr_t));
    memcpy(&(ip_pkt->ip_src.s_addr), &src_ip, sizeof(in_addr_t));
    ip_pkt->ip_sum = 0;

    memcpy(ip_pkt+1, data, data_len);

    return craft_eth(index, raw_addr, buff, src_mac, dst_mac, ip_pkt, data_len + sizeof(struct ip));

}

size_t craft_icmp(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], void* data, size_t data_len) {

    struct icmp* icmp_pkt = malloc(sizeof(struct icmp) + data_len);
    if(icmp_pkt == NULL) {
        _ERROR("%s\n", "malloc failed for icmp");
        exit(EXIT_FAILURE);
    }

    // ICMP header
    icmp_pkt->icmp_type = ICMP_ECHO;
    icmp_pkt->icmp_code = 0;
    icmp_pkt->icmp_id = htons (1000);
    icmp_pkt->icmp_seq = htons (0);
    icmp_pkt->icmp_cksum = 0;
    memcpy(icmp_pkt + 1, data, data_len);
    icmp_pkt->icmp_cksum = csum(icmp_pkt, sizeof(struct icmp) + data_len);

    return craft_eth(index, raw_addr, buff, src_mac, dst_mac, icmp_pkt, sizeof(struct icmp) + data_len);

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

/** Write "n" bytes to a descriptor.
* RETURN:
* bytes written or -1 on error
**/
ssize_t write_n(int fd, char *buf, size_t n) {
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
            buf += curr_n;
        }
    }
    return tot_n;
}

void print_hwa(char* mac, int len) {
    int n;
    char *fmt_first = "%02x", *fmt_rest = ":%02x";
    char *fmt = fmt_first;

    for(n = 0; n < len; n++) {
        printf(fmt, mac[n]);
        fmt = fmt_rest;
    }
}
