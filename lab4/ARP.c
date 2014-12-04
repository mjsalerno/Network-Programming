#include "ARP.h"

void handle_sigint(int sign) {
    /**
    * From signal(7):
    * POSIX.1-2004 (also known as POSIX.1-2001 Technical Corrigendum 2) requires an  implementation
    * to guarantee that the following functions can be safely called inside a signal handler:
    * ... _Exit() ... unlink() ...
    */
    sign++; /* for -Wall -Wextra -Werror */
    unlink(ARP_PATH);
    _Exit(EXIT_FAILURE);
}

int main() {
    struct hwa_info* hw_list = NULL;
    struct hwa_ip * mip_head = NULL;
    struct arp_cache* arp_lst = NULL;

    struct sockaddr_un unix_addr;
    struct sockaddr_ll raw_addr;

    int rawsock, unixsock, max_fd, listening_unix_sock;
    socklen_t raw_len, unix_len;
    struct arp_cache* tmp_arp;
    struct in_addr tmp_ip;
    unsigned char tmp_mac[ETH_ALEN];
    struct hwa_ip* tmp_hwa_ip;
    struct arphdr* arp_hdr_ptr;
    int erri;
    ssize_t errs;
    size_t arp_size;

    fd_set fdset;

    struct sigaction sigact;

    char buf[BUFSIZE];
    char arp_buf[BUFSIZE];
    unsigned char bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    hw_list = get_hw_addrs();
    keep_eth0(&hw_list, &mip_head);
    print_hw_addrs(hw_list);
    free_hwa_info(hw_list);
    print_hwa_list(mip_head);

    if(mip_head == NULL) {
        _ERROR("%s\n", "eth0 not found\n");
        exit(EXIT_FAILURE);
    }

    rawsock = socket(AF_PACKET, SOCK_RAW, htons(ARP_ETH_PROTO));
    if(rawsock < 0) {
        perror("There was an error creating the raw soket");
        exit(EXIT_FAILURE);
    }

    unixsock = socket(AF_LOCAL, SOCK_STREAM, 0);
    if(unixsock < 0) {
        perror("There was an error creating the unix soket");
        exit(EXIT_FAILURE);
    }

    memset(&raw_addr, 0, sizeof(struct sockaddr_ll));
    memset(&unix_addr, 0, sizeof(struct sockaddr_un));

    memcpy(raw_addr.sll_addr, mip_head->if_haddr, ETH_ALEN);
    raw_addr.sll_family = PF_PACKET;
    raw_addr.sll_protocol = htons(ARP_ETH_PROTO);
    raw_addr.sll_ifindex = mip_head->if_index;
    raw_addr.sll_halen = ETH_ALEN;
    raw_addr.sll_addr[6] = 0;
    raw_addr.sll_addr[7] = 0;

    strncpy(unix_addr.sun_path, ARP_PATH, sizeof(unix_addr.sun_path));
    _DEBUG("will bind to: %s\n", ARP_PATH);
    unix_addr.sun_family = AF_LOCAL;
    unix_addr.sun_path[sizeof(unix_addr.sun_path) - 1] = '\0';

    raw_len = sizeof(struct sockaddr_ll);
    unix_len = sizeof(struct sockaddr_un);

    unlink(ARP_PATH);
    erri = bind(unixsock, (struct sockaddr const *)&unix_addr, unix_len);
    if(erri != 0) {
        perror("arp.bind(unix)");
        close(unixsock);
        exit(EXIT_FAILURE);
    }

    /* set up the signal handler for SIGINT ^C */
    sigact.sa_handler = &handle_sigint;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    erri = sigaction(SIGINT, &sigact, NULL);
    if(erri < 0) {
        _ERROR("%s: %m\n", "sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }

    erri = listen(unixsock, 128);
    if(erri != 0) {
        perror("arp.listen(unix)");
        close(unixsock);
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

        if(FD_ISSET(rawsock, &fdset)) {                                                 /* RAW sock */
            raw_len = sizeof(struct sockaddr_ll);
            errs = recvfrom(rawsock, buf, sizeof(buf), 0, (struct sockaddr *)&raw_addr, &raw_len);
            if(errs < 0) {
                perror("arp.recvfrom(raw)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("%s\n", "Got something on the raw socket");

            arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);
            printf("got arp\n");
            print_arp(arp_hdr_ptr);

            if(arp_hdr_ptr->ar_op == ARPOP_REQUEST) {
                tmp_hwa_ip = is_my_ip(mip_head, *((in_addr_t *) extract_target_pa(arp_hdr_ptr)));
                if (tmp_hwa_ip != NULL) {
                    _DEBUG("%s\n", "somone is asking for my mac");

                    /*saving the senders mac*/
                    memcpy(tmp_mac, extract_sender_hwa(arp_hdr_ptr), ETH_ALEN);
                    memcpy(&tmp_ip, extract_sender_pa(arp_hdr_ptr), arp_hdr_ptr->ar_pln);

                    memset(buf, 0, sizeof(buf));
                    memset(arp_buf, 0, sizeof(arp_buf));
                    arp_size = craft_arp(arp_buf, ARP_ETH_PROTO, ARPOP_REPLY, ETHERTYPE_IP, ARPHRD_ETHER,
                            tmp_hwa_ip->if_haddr, (unsigned char *) &tmp_hwa_ip->ip_addr.sin_addr.s_addr, tmp_mac,
                            (unsigned char *) &tmp_ip);

                    craft_eth(buf, &raw_addr, tmp_hwa_ip->if_haddr, tmp_mac, tmp_hwa_ip->if_index);
                    memcpy(buf + sizeof(struct ethhdr), arp_buf, arp_size);

                    printf("sending arp\n");
                    print_arp((struct arphdr*)(arp_buf+2));

                    raw_len = sizeof(raw_addr);
                    errs = sendto(rawsock, buf, sizeof(struct ethhdr) + arp_size + 2, 0, (struct sockaddr const *)&raw_addr, raw_len);
                    if(errs < 0) {
                        perror("sendto(arpr)");
                        exit(EXIT_FAILURE);
                    }

                } else {
                    _DEBUG("%s\n", "not my mac");
                }
            } else if(arp_hdr_ptr->ar_op == ARPOP_REPLY) {
                /*todo: handle reply*/

            } else {
                _ERROR("Not sure what to do with arp_op: %d\n", arp_hdr_ptr->ar_op);
            }

        }

        if(FD_ISSET(unixsock, &fdset)) {                                                 /* UNIX sock */

            listening_unix_sock = accept(unixsock, (struct sockaddr*)&unix_addr, &unix_len);
            if(listening_unix_sock < 0) {
                perror("arp.connect(unixsock)");
                close(unixsock);
                exit(EXIT_FAILURE);
            }

            errs = recv(listening_unix_sock, buf, sizeof(buf), 0);
            if(errs < 0) {
                perror("arp.recv(unix)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("Got something on the unix socket, bytes:%d\n", (int)errs);
            memcpy(&tmp_ip.s_addr, buf, sizeof(uint32_t));
            _DEBUG("got areq for IP: %s\n", inet_ntoa(tmp_ip));

            tmp_arp = has_arp(arp_lst, (in_addr_t)*buf);

            if(tmp_arp != NULL) {
                _DEBUG("%s\n", "found a matching ip");
            } else {
                _DEBUG("%s\n", "did not find a matching ip, need to ask");
                memset(buf, 0, sizeof(buf));
                memset(arp_buf, 0, sizeof(arp_buf));
                arp_size = craft_arp(arp_buf, ARP_ETH_PROTO, ARPOP_REQUEST, ETHERTYPE_IP, ARPHRD_ETHER, mip_head->if_haddr, (unsigned char*)&mip_head->ip_addr.sin_addr.s_addr, NULL, (unsigned char*)&tmp_ip);
                craft_eth(buf, &raw_addr, mip_head->if_haddr, bcast_mac, mip_head->if_index);
                memcpy(buf + sizeof(struct ethhdr), arp_buf, arp_size);

                arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);
                struct in_addr ip_struc;
                ip_struc.s_addr = *(in_addr_t*) extract_target_pa(arp_hdr_ptr);
                printf("\nwill ask for this ip: %s\n", inet_ntoa(ip_struc));

                printf("Sending on mac: ");
                print_hwa(raw_addr.sll_addr, 6);
                printf("\n");
                printf("Sending on index: %d\n\n", raw_addr.sll_ifindex);

                printf("eth to mac: ");
                print_hwa(((struct ethhdr*)buf)->h_dest, 6);
                printf("\n");
                printf("eth from mac: ");
                print_hwa(((struct ethhdr*)buf)->h_source, 6);
                printf("\n");

                printf("sending arp\n");
                print_arp((struct arphdr*)(arp_buf+2));

                raw_len = sizeof(raw_addr);
                errs = sendto(rawsock, buf, sizeof(struct ethhdr) + arp_size + 2, 0, (struct sockaddr const *)&raw_addr, raw_len);
                if(errs < 0) {
                    perror("sendto(arpr)");
                    exit(EXIT_FAILURE);
                }

            }

        }

    }

    return 1;

}
