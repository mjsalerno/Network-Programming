#include <tclTomMath.h>
#include "get_hw_addrs.h"

int main() {
    struct hwa_info* hw_list = NULL;
    struct hwa_ip * mip_head = NULL;

    struct sockaddr_un unix_addr;
    struct sockaddr_ll raw_addr;

    int rawsock, unixsock, max_fd;
    socklen_t raw_len, unix_len;
    int erri;
    ssize_t errs;

    fd_set fdset;

    char buf[BUFSIZE];

    hw_list = get_hw_addrs();
    keep_eth0(&hw_list, &mip_head);
    print_hw_addrs(hw_list);
    free_hwa_info(hw_list);
    print_hwa_list(mip_head);

    rawsock = socket(AF_PACKET, SOCK_RAW, htons(PROTO));
    if(rawsock < 3) {
        perror("There was an error creating the raw soket");
        exit(EXIT_FAILURE);
    }

    unixsock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(unixsock < 3) {
        perror("There was an error creating the unix soket");
        exit(EXIT_FAILURE);
    }

    memset(&raw_addr, 0, sizeof(struct sockaddr_ll));
    memset(&unix_addr, 0, sizeof(struct sockaddr_un));

    memcpy(raw_addr.sll_addr, mip_head->if_haddr, ETH_ALEN);
    raw_addr.sll_family = PF_PACKET;
    raw_addr.sll_protocol = htons(PROTO);
    raw_addr.sll_ifindex = mip_head->if_index;
    raw_addr.sll_halen = ETH_ALEN;
    raw_addr.sll_addr[6] = 0;
    raw_addr.sll_addr[7] = 0;

    strncpy(unix_addr.sun_path, ARP_PATH, sizeof(unix_addr.sun_family));
    unix_addr.sun_family = AF_LOCAL;
    unix_addr.sun_path[sizeof(unix_addr.sun_path) - 1] = '\0';

    sizeof(struct sockaddr_ll);
    unix_len = sizeof(struct sockaddr_un);

    erri = connect(unixsock, (struct sockaddr*)&unix_addr, unix_len);
    if(erri < 0) {
        perror("arp.connect(unixsock)");
        exit(EXIT_FAILURE);
    }

    max_fd = MAX(rawsock, unixsock);
    FD_ZERO(&fdset);

    for(EVER) {

        FD_SET(rawsock, &fdset);
        FD_SET(unixsock, &fdset);

        erri = select(max_fd + 1, &fdset, NULL, NULL, NULL);
        if(erri < 0) {
            perror("arp.select()");
            break;
        }

        if(FD_ISSET(rawsock, &fdset)) {
            errs = recvfrom(rawsock, buf, sizeof(buf), 0, (struct sockaddr *)&raw_addr, &raw_len);
            if(errs < 0) {
                perror("recvfrom(raw)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("Got something on the raw socket");
        }

        if(FD_ISSET(unixsock, &fdset)) {
            errs = recvfrom(unixsock, buf, sizeof(buf), 0, (struct sockaddr *)&unix_addr, &unix_len);
            if(errs < 0) {
                perror("recvfrom(raw)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("Got something on the unix socket");
        }
    }

    return 1;

}