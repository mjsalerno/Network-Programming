#include "ARP.h"

void handle_rep(char* buf);
void handle_req(int rawsock, char* buf);
void ans_tour(int sock, struct arp_cache* entry);
void handle_sigint(int sign);

static struct hwa_info* hw_list = NULL;
static struct hwa_ip * mip_head = NULL;
static struct arp_cache* arp_lst = NULL;
static char* buf;

int main() {

    struct sockaddr_un unix_addr;
    struct sockaddr_ll raw_addr;

    int rawsock, unixsock, max_fd, accepted_unix_sock;
    socklen_t raw_len, unix_len;
    struct arp_cache* tmp_arp;
    struct in_addr tmp_ip;
    struct arphdr* arp_hdr_ptr;
    int erri;
    ssize_t errs;
    size_t arp_size;

    fd_set fdset;

    struct sigaction sigact;

    char arp_buf[BUFSIZE];
    unsigned char bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    hw_list = get_hw_addrs();
    keep_eth0(&hw_list, &mip_head);
    #ifdef DEBUG
    print_hw_addrs(hw_list);
    #endif
    free_hwa_info(hw_list);
    printf("Found " IFACE_TO_KEEP "\n");
    print_hwa_list(mip_head);

    buf = malloc(BUFSIZE);
    if(buf == NULL) {
        _ERROR("%s\n", "malloc for buf failed");
        exit(EXIT_FAILURE);
    }

    if(mip_head == NULL) {
        _ERROR("%s\n", IFACE_TO_KEEP" not found\n");
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
            errs = recvfrom(rawsock, buf, BUFSIZE, 0, (struct sockaddr *)&raw_addr, &raw_len);
            if(errs < 0) {
                perror("arp.recvfrom(raw)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("%s\n", "Got something on the raw socket");

            if(ntohs(*(uint16_t*)(buf + sizeof(struct ethhdr))) != ARP_ETH_PROTO) {
                _SPEC("got someone elses ARP, proto: 0x%x , wanted: 0x%x, skipping\n", ntohs(*(uint16_t*)(buf + sizeof(struct ethhdr))), ARP_ETH_PROTO);
                continue;
            }

            arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);
            #ifdef DEBUG
            printf("got arp\n");
            print_arp(arp_hdr_ptr);
            #endif

            if(ntohs(arp_hdr_ptr->ar_op) == ARPOP_REQUEST) {
                tmp_ip.s_addr = *(in_addr_t*) extract_target_pa(arp_hdr_ptr);
                _INFO("got a request for IP: %s\n", inet_ntoa(tmp_ip));
                handle_req(rawsock, buf);

            } else if(ntohs(arp_hdr_ptr->ar_op) == ARPOP_REPLY) {
                tmp_ip.s_addr = *(in_addr_t*)extract_target_pa(arp_hdr_ptr);
                _INFO("got a reply for IP: %s\n", inet_ntoa(tmp_ip));
                handle_rep(buf);

            } else {
                _ERROR("Not sure what to do with arp_op: %d\n", ntohs(arp_hdr_ptr->ar_op));
            }

        }

        if(FD_ISSET(unixsock, &fdset)) {                                                 /* UNIX sock */

            accepted_unix_sock = accept(unixsock, (struct sockaddr*)&unix_addr, &unix_len);
            if(accepted_unix_sock < 0) {
                perror("arp.connect(unixsock)");
                close(unixsock);
                exit(EXIT_FAILURE);
            }

            errs = recv(accepted_unix_sock, buf, BUFSIZE, 0);
            if(errs < 0) {
                perror("arp.recv(unix)");
                exit(EXIT_FAILURE);
            }
            _DEBUG("Got something on the unix socket, bytes:%d\n", (int)errs);
            memcpy(&tmp_ip.s_addr, buf, sizeof(uint32_t));
            _SPEC("got areq from my tour for IP: %s\n", inet_ntoa(tmp_ip));

            tmp_arp = get_arp(arp_lst, *(in_addr_t*)buf);

            if(tmp_arp != NULL) {
                _DEBUG("%s\n", "found a matching ip");
                /*ans tour*/
                arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);
                printf("sending arep to my tour from my cache\n");
                ans_tour(accepted_unix_sock, tmp_arp);
            } else {
                _DEBUG("%s\n", "did not find a matching ip, need to ask");

                /*make partial entry*/
                add_part_arp(&arp_lst, tmp_ip.s_addr, accepted_unix_sock);

                memset(buf, 0, BUFSIZE);
                memset(arp_buf, 0, sizeof(arp_buf));
                arp_size = craft_arp(arp_buf, htons(ARP_ETH_PROTO), ARPOP_REQUEST, ETHERTYPE_IP, ARPHRD_ETHER, mip_head->if_haddr, (unsigned char*)&mip_head->ip_addr.sin_addr.s_addr, NULL, (unsigned char*)&tmp_ip);
                craft_eth(buf, ARP_ETH_PROTO, &raw_addr, mip_head->if_haddr, bcast_mac, mip_head->if_index);
                memcpy(buf + sizeof(struct ethhdr), arp_buf, arp_size);

                arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);
                tmp_ip.s_addr = *(in_addr_t*) extract_target_pa(arp_hdr_ptr);
                _DEBUG("\nwill ask for this ip: %s\n", inet_ntoa(tmp_ip));

                #ifdef DEBUG
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
                _SPEC("sending proto: 0x%x , ours: 0x%x\n", ntohs(*(uint16_t*)(buf + sizeof(struct ethhdr))), ARP_ETH_PROTO);
                #endif
                printf("sending out areq\n");
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


void handle_rep(char* buf) {
    struct arphdr* arp_hdr_ptr;
    struct arp_cache* arp_c;

    arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);

    arp_c = get_arp(arp_lst, *(in_addr_t*)extract_sender_pa(arp_hdr_ptr));
    if(arp_c == NULL || arp_c->fd < 0) {
        _ERROR("%s\n", "got a reply for somthing I never asked for");
        if(arp_c == NULL) {
            _ERROR("%s\n", "arp_c was NULL");
        } else {
            _ERROR("fd: %d\n", arp_c->fd);
        }
        exit(EXIT_FAILURE);
    }

    /*update partial entry*/
    memcpy(arp_c->hw.dst_addr, extract_sender_hwa(arp_hdr_ptr), ETH_ALEN);

    /*ans tour*/
    ans_tour(-1, arp_c);

}

void handle_req(int rawsock, char* buf) {

    struct sockaddr_ll raw_addr;
    struct in_addr tmp_ip;
    unsigned char tmp_mac[ETH_ALEN];
    struct hwa_ip* tmp_hwa_ip;
    struct arphdr* arp_hdr_ptr;
    socklen_t raw_len;
    char arp_buf[BUFSIZE];
    ssize_t errs;
    size_t arp_size;

    arp_hdr_ptr = (struct arphdr*)(buf + sizeof(struct ethhdr) + 2);

    tmp_hwa_ip = is_my_ip(mip_head, *((in_addr_t *) extract_target_pa(arp_hdr_ptr)));
    if (tmp_hwa_ip != NULL) {
        printf("somone is asking for my mac, sending arp\n");

        /*saving the senders mac*/
        memcpy(tmp_mac, extract_sender_hwa(arp_hdr_ptr), ETH_ALEN);
        memcpy(&tmp_ip, extract_sender_pa(arp_hdr_ptr), arp_hdr_ptr->ar_pln);

        memset(buf, 0, BUFSIZE);
        memset(arp_buf, 0, sizeof(arp_buf));
        arp_size = craft_arp(arp_buf, htons(ARP_ETH_PROTO), ARPOP_REPLY, ETHERTYPE_IP, ARPHRD_ETHER,
                tmp_hwa_ip->if_haddr, (unsigned char *) &tmp_hwa_ip->ip_addr.sin_addr.s_addr, tmp_mac,
                (unsigned char *) &tmp_ip);

        craft_eth(buf, ARP_ETH_PROTO, &raw_addr, tmp_hwa_ip->if_haddr, tmp_mac, tmp_hwa_ip->if_index);
        memcpy(buf + sizeof(struct ethhdr), arp_buf, arp_size);

        #ifdef DEBUG
        print_arp((struct arphdr*)(arp_buf+2));
        #endif

        raw_len = sizeof(raw_addr);
        errs = sendto(rawsock, buf, sizeof(struct ethhdr) + arp_size + 2, 0, (struct sockaddr const *)&raw_addr, raw_len);
        if(errs < 0) {
            perror("sendto(arpr)");
            exit(EXIT_FAILURE);
        }

    } else {
        _DEBUG("%s\n", "not my mac");
    }
}

void ans_tour(int sock, struct arp_cache* entry) {
    struct hwaddr answer;
    ssize_t errs;

    if(sock < 0)
        sock = entry->fd;

    #ifdef DEBUG
    _DEBUGY("sock: %d\n", sock);
    print_hwaddr(&entry->hw);
    _DEBUGY("%s\n", "END");
    #endif

    /*ans tour*/
    memcpy(&answer, &entry->hw, sizeof(struct hwaddr));
    answer.dst_halen = ETH_ALEN;
    answer.src_halen = ETH_ALEN;
    memcpy(answer.src_addr, mip_head->if_haddr, ETH_ALEN);
    memcpy(answer.dst_addr, entry->hw.dst_addr, ETH_ALEN);
    answer.src_ifindex = mip_head->if_index;
    print_hwaddr(&answer);

    errs = send(sock, &answer, sizeof(struct hwaddr), 0);
    if(errs < 0) {
        _ERROR("%s %m\n", "send:");
    }

    if( close(sock) ) {
        _ERROR("%s %m\n", "close:");
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "sent the answer to tour");

    entry->fd = -1;
}

void handle_sigint(int sign) {
    /**
    * From signal(7):
    * POSIX.1-2004 (also known as POSIX.1-2001 Technical Corrigendum 2) requires an  implementation
    * to guarantee that the following functions can be safely called inside a signal handler:
    * ... _Exit() ... unlink() ...
    */
    sign++; /* for -Wall -Wextra -Werror */
    free(buf);
    free_hwa_ip(mip_head);
    free_arp_cache(arp_lst);
    unlink(ARP_PATH);
    _Exit(EXIT_FAILURE);
}
