#include "ODR.h"
#include "debug.h"

static struct tbl_entry route_table[NUM_NODES];

int notmain(void) {


    ssize_t n;
    struct hwa_info *hwahead;
    int rawsock;
    socklen_t  len;

    /* raw socket vars*/
    struct sockaddr_ll raw_addr;

    //bcast
    unsigned char dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    //mine
    unsigned char src_mac[6] = {0x5c, 0x51, 0x4f, 0x11, 0x25, 0x65};
    unsigned char dst_mac[6] = {0x5c, 0x51, 0x4f, 0x11, 0x25, 0x65};
    int index = 1;

    //vm1 eth2 index
    //unsigned char src_mac[6] = {0x00, 0x0c, 0x29, 0x49, 0x3f, 0x65};
    //unsigned char dst_mac[6] = {0x00, 0x0c, 0x29, 0x49, 0x3f, 0x65};
    //int index = 3;

    //vm1 eth2
    //unsigned char src_mac[6] = {0x00,0x0c,0x29,0x49,0x3f,0x6f};
    //unsigned char dst_mac[6] = {0x00,0x0c,0x29,0x49,0x3f,0x6f};

    //vm2 eth1
    //unsigned char src_mac[6] = {0x00, 0x0c,0x29, 0xd9, 0x08, 0xf6};
    //unsigned char dst_mac[6] = {0x00, 0x0c,0x29, 0xd9, 0x08, 0xf6};
    //int index = 3;

    char* buff = malloc(ETH_FRAME_LEN);

    memset(buff, 0, sizeof(ETH_FRAME_LEN));
    memset(route_table, 0, NUM_NODES);

    add_route(route_table, "192.168.1.1", src_mac, 1, 1, 1, 1);
    add_route(route_table, "192.168.1.2", src_mac, 1, 1, 1, 1);
    find_route_index(route_table, "192.168.1.1");
    find_route_index(route_table, "192.168.1.2");
    find_route_index(route_table, "192.168.1.3");

    if ((hwahead = get_hw_addrs()) == NULL) {       /* get MAC addrs of our interfaces */
        fprintf(stderr, "ERROR: get_hw_addrs()\n");
        exit(EXIT_FAILURE);
    }
    print_hw_addrs(hwahead);
    rm_eth0_lo(&hwahead);
    print_hw_addrs(hwahead);
    if(hwahead == NULL) {
        _ERROR("%s", "You've got no interfaces left!\n");
        exit(EXIT_FAILURE);
    }

    rawsock = socket(AF_PACKET, SOCK_RAW, htons(PROTO));
    if(rawsock < 0) {
        perror("ERROR: socket(RAW)");
        exit(EXIT_FAILURE);
    }

    /* todo: sending packet for test, remove it */
    _DEBUG("%s", "sending packet...\n");
    craft_frame(rawsock, index, &raw_addr, buff, src_mac, dst_mac, "sup", 4);
    printf("sendinf over index: %d\n", raw_addr.sll_ifindex);
    printf("from mac: (%02X:%02X:%02X:%02X:%02X:%02X)\n",
            src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
    printf("to mac: (%02X:%02X:%02X:%02X:%02X:%02X)\n",
            raw_addr.sll_addr[0], raw_addr.sll_addr[1], raw_addr.sll_addr[2], raw_addr.sll_addr[3],
            raw_addr.sll_addr[4], raw_addr.sll_addr[5]);

    n = sendto(rawsock, buff, ETH_FRAME_LEN, 0, (struct sockaddr const *) &raw_addr, sizeof(struct sockaddr_ll));
    if(n < 1) {
        perror("sendto(RAW)");
        exit(EXIT_FAILURE);
    }

    memset(&raw_addr, 0, sizeof(struct sockaddr_ll));
    len = sizeof(struct sockaddr_ll);
    n = recvfrom(rawsock, buff, ETH_FRAME_LEN, 0, (struct sockaddr*)&raw_addr, &len);
    if(n < 1) {
        perror("error");
        exit(EXIT_FAILURE);
    }
    printf("done: %s\n", buff + sizeof(struct ethhdr));
    /*fixme ^^*/
    return 1;
}