#include "ping.h"



/**
* fills in the raw packet with the src and dst mac addresses.
* appends the data to the end of the eth hdr.
* fills in the sockaddr_ll in case sendto will be used.
* CAN BE NULL if sendto will not be used.
*
* returns a pointer to the new packet (the thing you already have)
* returns NULL if there was an error (that's a lie)
*/
size_t craft_icmp_pkt(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN],
        unsigned char dst_mac[ETH_ALEN], in_addr_t* ip_src, in_addr_t* ip_dst, void* data, size_t data_len) {

    struct ethhdr* et = buff;
    struct ip* iph = (struct ip*)((et) + 1);
    //struct icmp* icmph = (struct icmp*)(iph + 1);

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

    /*IPv4 header*/
    iph->ip_hl = IP4_HDRLEN / sizeof (uint32_t);
    iph->ip_v = 4;
    iph->ip_tos = 0;
    iph->ip_len = htons((uint16_t)(IP4_HDRLEN + ICMP_HDRLEN + data_len));
    iph->ip_id = htons(0);
    iph->ip_off = IP_DF;
    iph->ip_ttl = 255;
    iph->ip_p = IPPROTO_ICMP;
    memcpy(&(iph->ip_dst.s_addr), ip_dst, sizeof(in_addr_t));
    memcpy(&(iph->ip_src.s_addr), ip_src, sizeof(in_addr_t));
    iph->ip_sum = '?';



    /* copy in the data and ethhdr */
    memcpy(buff + sizeof(struct ethhdr), data, data_len);

    _DEBUG("crafted frame with proto: %d\n", et->h_proto);
    return sizeof(struct ethhdr) + data_len;
}

/*calculates the checksum of an IP packet*/
uint16_t ip_csum(struct ip* iph, size_t len) {
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)iph;

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

