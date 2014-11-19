#include "ODR.h"
#include "debug.h"

/* fixme: remove */
static char host_ip[INET_ADDRSTRLEN] = "127.0.0.1";
static char host_name[BUFF_SIZE];
static struct tbl_entry route_table[NUM_NODES];

int main(int argc, char *argv[]) {
    int err;
    ssize_t n;
    struct hwa_info *hwahead;
    int unixsock, rawsock, stdinfd;
    struct sockaddr_un unix_addr, local_addr;
    socklen_t len;
    struct svc_entry svcs[SVC_MAX_NUM];
    char *buf_msg;                    /* buffer for local messages */
    struct odr_msg *msgp;               /* to cast buf_local_msg */
    struct odr_msg *out_msg;
    uint32_t broadcastID = 0;
    struct msg_queue queue;
    int staleness;

    /* raw socket vars*/
    struct sockaddr_ll raw_addr;

    /* select(2) vars */
    fd_set rset;
    int fpd1;

    if(argc != 2) {
        fprintf(stderr, "usage: %s staleness\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    staleness = atoi(argv[1]);

    char* buff = malloc(ETH_FRAME_LEN);
    buf_msg = malloc(ODR_MSG_MAX);
    if(buf_msg == NULL) {
        _ERROR("%s", "malloc() returned NULL\n");
        exit(EXIT_FAILURE);
    }
    out_msg = malloc(ODR_MSG_MAX);
    if(out_msg == NULL) {
        _ERROR("%s", "malloc() returned NULL\n");
        exit(EXIT_FAILURE);
    }

    memset(buff, 0, sizeof(ETH_FRAME_LEN));
    memset(&unix_addr, 0, sizeof(unix_addr));
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&raw_addr, 0, sizeof(raw_addr));
    memset(out_msg, 0, ODR_MSG_MAX);

    if ((hwahead = get_hw_addrs()) == NULL) {       /* get MAC addrs of our interfaces */
        fprintf(stderr, "ERROR: get_hw_addrs()\n");
        exit(EXIT_FAILURE);
    }
    rm_eth0_lo(&hwahead);
    print_hw_addrs(hwahead);
    if(hwahead == NULL) {
        _ERROR("%s", "You've got no interfaces left!\n");
        exit(EXIT_FAILURE);
    }

    rawsock = socket(AF_PACKET, SOCK_RAW, htons(PROTO));
    if(rawsock < 0) {
        perror("ERROR: socket(RAW)");
        free(buf_msg);
        free(out_msg);
        free(buff);
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }

    unlink(ODR_PATH); /* unlink before dropping root */

/*    if(setuid(1000) < 0) {
        _ERROR("setuid(%d): %m\n", 1000);
        exit(EXIT_FAILURE);
    }*/


    unixsock = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if (unixsock < 0) {
        perror("ERROR: socket()");
        close(rawsock);
        free(buf_msg);
        free(out_msg);
        free(buff);
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }


    unix_addr.sun_family = AF_LOCAL;
    strncpy(unix_addr.sun_path, ODR_PATH, sizeof(unix_addr.sun_path)-1);

    err = bind(unixsock, (struct sockaddr*) &unix_addr, (socklen_t)sizeof(unix_addr));
    if(err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    err = gethostname(host_name, sizeof(host_name));  /* get my hostname */
    if(err < 0) {
        perror("ERROR: gethostname()");
        goto cleanup;
    }
    _NOTE("ODR at node %s (\"CanonicalIP\" %s): ready....\n", host_name, host_ip);

    svc_init(svcs, SVC_MAX_NUM);   /* ready to accept local msgs */
    queue.head = NULL;             /* init the queue */

    FD_ZERO(&rset);
    stdinfd = fileno(stdin);
    fpd1 = MAX(MAX(unixsock, rawsock), stdinfd) + 1;
    for(EVER) {
        _DEBUG("%s", "waiting for messages....\n");
        FD_SET(unixsock, &rset);
        FD_SET(rawsock, &rset);
        FD_SET(stdinfd, &rset);
        err = select(fpd1, &rset, NULL, NULL, NULL);
        if(err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if(FD_ISSET(unixsock, &rset)) {
            len = sizeof(local_addr);
            n = recvfrom(unixsock, buf_msg, ODR_MSG_MAX, 0, (struct sockaddr*)&local_addr, &len);
            if(n < 0) {
                perror("ERROR: recvfrom(unixsock)");
                goto cleanup;
            } else if(n < (ssize_t)sizeof(struct odr_msg)) {
                _ERROR("recv'd odr_msg was too short!! n: %d\n", (int)n);
                goto cleanup;
            }
            msgp = (struct odr_msg*) buf_msg;
            _DEBUG("GOT msg from socket: %s, with dest: %s:%d\n",
                    local_addr.sun_path, msgp->dst_ip, msgp->dst_port);
            /* Get/Assign this service a port number */
            msgp->src_port = (uint16_t) svc_update(svcs, &local_addr);
            strcpy(msgp->src_ip, host_ip);

            if(0 == strcmp(msgp->dst_ip, host_ip)) {
                /* the destination IP of this svc's mesg is local */
                _DEBUG("%s", "msg has local dest IP\n");
                deliver_app_mesg(unixsock, svcs, msgp);
            } else {
                int route_i;
                _DEBUG("msg has NON-LOCAL dst IP: %s, host IP: %s\n", msgp->dst_ip, host_ip);
                /* force_rediscid off  AND     we do have a route to the dest IP */
                if(!msgp->force_redisc &&
                        -1 != (route_i = find_route_index(route_table, msgp->dst_ip))) {
                    /* send DATA */
                    send_on_iface(rawsock, hwahead, msgp, route_table[route_i].iface_index, route_table[route_i].mac_next_hop);
                    /* done ? */
                } else {
                    /* send RREQ */
                    /* either we don't have a route or force_redisc was set
                     so we pretend like we don't have a route */
                    queue_store(&queue, msgp);
                    memset(out_msg, 0, ODR_MSG_MAX);
                    craft_rreq(out_msg, host_ip, msgp->dst_ip, msgp->force_redisc, broadcastID);
                    broadcastID++;
                    broadcast(rawsock, hwahead, out_msg, -1);
                    /* done ? */
                }
            }
            /* note: invalidate ptr to buf_msg just to be safe*/
            msgp = NULL;
        } else if(FD_ISSET(rawsock, &rset)) {   /* something on the raw socket */
            int eff, its_me, forw_index;
            uint8_t we_sent;

            _DEBUG("%s\n", "The raw socket has something in it ..");
            len = sizeof(raw_addr);
            n = recvfrom(rawsock, buf_msg, ODR_MSG_MAX, 0, (struct sockaddr*)&raw_addr, &len);
            if(n < 0) {
                perror("ERROR: recvfrom(rawsock)");
                goto cleanup;
            } else if(n < (ssize_t)sizeof(struct odr_msg)) {
                _ERROR("recv'd odr_msg was too short!! n: %d\n", (int)n);
                goto cleanup;
            }

            if(htons(raw_addr.sll_protocol) != PROTO) {
                _ERROR("Got bad proto: %d, we are: %d\n", raw_addr.sll_protocol, PROTO);
                continue;
            }

            msgp = (struct odr_msg*) (buf_msg +sizeof(struct ethhdr));
            ntoh_odr_msg(msgp);
            msgp->num_hops++;
            its_me = (0 == strcmp(msgp->dst_ip, host_ip));
            _DEBUG("its_me: %d\n", its_me);
            len = sizeof(raw_addr);

            _DEBUG("msg type: %d\n", msgp->type);
            switch(msgp->type) {

                case T_RREQ:
                    _DEBUG("%s\n", "fell into case T_RREQ");
                    eff = 0;
                    we_sent = 0;
                    err = add_route(route_table, msgp, &raw_addr, staleness, &eff,
                            rawsock, hwahead, &queue);
                    forw_index = find_route_index(route_table, msgp->dst_ip);

                    if(err < 0) {
                        _DEBUG("%s\n", "the route was not added");
                    } else {

                        _DEBUG("%s\n", "added the route");
                        if(!msgp->do_not_rrep && (!msgp->force_redisc || its_me)) {
                            _DEBUG("%s\n", "going to send an RREP");
                            if(its_me) {
                                _DEBUG("%s\n", "crafted a rrep for me");
                                craft_rrep(out_msg, host_ip, msgp->src_ip, msgp->force_redisc, 0);
                                we_sent = 1;
                            } else if (forw_index > -1) {   /* we have the route */
                                _DEBUG("%s\n", "crafted a rrep since i know where it is");
                                craft_rrep(out_msg, host_ip, msgp->src_ip, msgp->force_redisc, route_table[forw_index].num_hops);
                                we_sent = 1;
                            } else {
                                _DEBUG("%s\n", "Could send if route known, but I don't know it");
                            }
                            if(we_sent) {
                                send_on_iface(rawsock, hwahead, out_msg, raw_addr.sll_ifindex, raw_addr.sll_addr);
                                _DEBUG("%s\n", "we sent it");
                            }
                        }

                        if(we_sent || eff) {
                            msgp->do_not_rrep = we_sent;
                            _DEBUG("flooding out the good news except for index: %d\n", raw_addr.sll_ifindex);
                            broadcast(rawsock, hwahead, msgp, raw_addr.sll_ifindex);
                        }
                    }

                    break;
                case T_RREP:
                    _DEBUG("%s\n", "fell into case T_RREP");
                    /* forwarding RREP */
                    if(!its_me) {
                        _DEBUG("%s\n", "it is not for me");
                        /* then i should still have the path made from the request (unless staleness is really low)*/
                        forw_index = find_route_index(route_table, msgp->dst_ip);
                        _DEBUG("forw_index: %d\n", forw_index);
                        if(forw_index < 0) {
                            _NOTE("%s\n", "there was an error, no route found to forward RREP maybe staleness too low");
                            /*exit(EXIT_FAILURE);*/
                            craft_rreq(out_msg, host_ip, msgp->dst_ip, msgp->force_redisc, broadcastID++);
                            broadcast(rawsock, hwahead, out_msg, raw_addr.sll_ifindex);
                            queue_store(&queue, out_msg);
                            break;
                        }

                        /*todo see if the eff gets updated like i think it will*/
                        eff = 0;
                        /* check to see if the their path is better than mine */
                        /* if force_redesc then always send theirs            */
                        if(add_route(route_table, msgp, &raw_addr, staleness, &eff, rawsock, hwahead, &queue)
                                || eff || msgp->force_redisc) {
                            _DEBUG("%s\n", "forwarding their route");
                            send_on_iface(rawsock, hwahead, msgp, route_table[forw_index].iface_index, route_table[forw_index].mac_next_hop);
                        } else { /*i have better route*/
                            /*todo i think this else is junk, I should never get here*/
                            _DEBUG("I have a better route, their hops: %d, my hops: %d\n", msgp->num_hops, route_table[forw_index].num_hops);
                            craft_rrep(out_msg, host_ip, msgp->dst_ip, 0, route_table[forw_index].num_hops);
                            send_on_iface(rawsock, hwahead, out_msg, route_table[forw_index].iface_index, route_table[forw_index].mac_next_hop);
                        }
                    /* the rrep is for me :D */
                    } else {
                        _DEBUG("%s\n", "it is for me :D");
                        eff = 0;
                        err = add_route(route_table, msgp, &raw_addr, staleness, &eff,
                                rawsock, hwahead, &queue);
                        if(err < 0 || eff < 1) {
                            _ERROR("got a RREP for me but it was not added to the table, err: %d  eff: %d\n", err, eff);
                        }
                    }
                    break;
                case T_DATA:
                    _DEBUG("%s\n", "fell into case T_DATA");
                    err = add_route(route_table, msgp, &raw_addr, staleness, &eff,
                            rawsock, hwahead, &queue);
                    _DEBUG("added route result: %d\n", err);

                    if(its_me) {
                        _DEBUG("%s\n", "received data for me");
                        deliver_app_mesg(unixsock, svcs, msgp);
                    } else if((forw_index = find_route_index(route_table, msgp->dst_ip)) > -1) {
                        _DEBUG("forw_index: %d\n", forw_index);
                        _DEBUG("%s\n", "received data that is not mine, and i have the route\n");
                        send_on_iface(rawsock, hwahead, msgp, route_table[forw_index]
                                .iface_index, route_table[forw_index].mac_next_hop);
                    } else {
                        _DEBUG("%s\n", "received data that is not for me, I do NOT have the route");
                        craft_rreq(out_msg, host_ip, msgp->dst_ip, 0, broadcastID++);
                        queue_store(&queue, msgp);
                    }
                    break;
                default:
                    _ERROR("Do not know what to do with this type of msg: %d\n", msgp->type);
                    break;

            }


        } else if(FD_ISSET(stdinfd, &rset)) {                   /* so user can ^D to terminate */
            char *errc = fgets(buf_msg, ODR_MSG_MAX, stdin);
            if (errc == NULL) {
                if(ferror(stdin)) {
                    fprintf(stderr, "ERROR: fgets() error from stdin\n");
                    goto cleanup;
                } else {
                    printf("Read ^D or EOF, terminating...\n");
                    break;
                }
            }
        }
    }

    free(buff);
    free(buf_msg);
    free(out_msg);
    close(unixsock);
    close(rawsock);
    unlink(ODR_PATH);
    free_hwa_info(hwahead); /* FREE HW list*/
    exit(EXIT_SUCCESS);
cleanup:
    free(buff);
    free(buf_msg);
    free(out_msg);
    close(unixsock);
    close(rawsock);
    unlink(ODR_PATH);
    free_hwa_info(hwahead);
    exit(EXIT_FAILURE);
}


/**
* Demultiplexes from the dest port in m{} to the correct  service to send to.
* Called when a T_DATA message is recv()'d with a local dest IP.
*
*/
void deliver_app_mesg(int unixfd, struct svc_entry *svcs, struct odr_msg *m) {
    struct sockaddr_un dst_addr;
    socklen_t len;
    ssize_t err;
    if(svcs == NULL || m == NULL) {
        _ERROR("%s", "svcs or m is NULL, terminating.\n");
        exit(EXIT_FAILURE);
    }

    if(m->dst_port > SVC_MAX_NUM) {
        _ERROR("service trying to send to a bad port: %d, ignoring...\n", m->dst_port);
        return;
    } else if(svcs[m->dst_port].port == -1) {
        /* the destination service doesn't exist, pretend like we sent it */
        _ERROR("no service at dst port: %d, ignoring...\n", m->dst_port);
        return;
    }
    dst_addr.sun_family = AF_LOCAL;
    strncpy(dst_addr.sun_path, svcs[m->dst_port].sun_path, sizeof(dst_addr.sun_path));
    dst_addr.sun_path[sizeof(dst_addr.sun_path)-1] = '\0';

    len = sizeof(dst_addr);
    err = sendto(unixfd, m, (sizeof(struct odr_msg) + m->len), 0, (struct sockaddr*)&dst_addr, len);
    if(err < 0) {
        /* fixme: which sendto() errors should we actually fail on? */
        _ERROR("%s %m. Ignoring error....\n", "sendto():");
        return;
    }
    _DEBUG("Relayed msg: sendto() local sock: %s\n", dst_addr.sun_path);
    return;
}

/* print the info about all interfaces in the hwa_info linked list */
void print_hw_addrs(struct hwa_info	*hwahead) {
    struct hwa_info	*hwa_curr;
    struct sockaddr	*sa;
    char   *ptr, straddrbuf[INET6_ADDRSTRLEN];
    int    i, prflag;

    hwa_curr = hwahead;

    printf("\n");
    for(; hwa_curr != NULL; hwa_curr = hwa_curr->hwa_next) {

        printf("%s :%s", hwa_curr->if_name, ((hwa_curr->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");

        if ((sa = hwa_curr->ip_addr) != NULL) {
            switch(sa->sa_family) {
                case AF_INET:
                    inet_ntop(AF_INET, &((struct sockaddr_in *) sa)->sin_addr, straddrbuf, INET_ADDRSTRLEN);
                    printf("         IP addr = %s\n", straddrbuf);
                    break;
                case AF_INET6:
                    inet_ntop(AF_INET6, &((struct sockaddr_in6 *) sa)->sin6_addr, straddrbuf, INET6_ADDRSTRLEN);
                    printf("         IP6 addr = %s\n", straddrbuf);
                    break;
                default:
                    break;
            }
        }

        prflag = 0;
        i = 0;
        do {
            if(hwa_curr->if_haddr[i] != '\0') {
                prflag = 1;
                break;
            }
        } while(++i < IFHWADDRLEN);

        if(prflag) {                        /* print the MAC address ex) FF:FF:FF:FF:FF:FF */
            printf("         HW addr = ");
            ptr = hwa_curr->if_haddr;
            i = IFHWADDRLEN;
            do {
                printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
            } while(--i > 0);
            printf("\n");
        }

        printf("         interface index = %d\n\n", hwa_curr->if_index);
    }
}

/**
* fills in the raw packet with the src and dst mac addresses.
* appends the data to the end of the eth hdr.
* fills in the sockaddr_ll in case sendto will be used.
* CAN BE NULL if sendto will not be used.
*
* returns a pointer to the new packet (the thing you already have)
* returns NULL if there was an error (that's a lie)
*/
size_t craft_frame(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], struct odr_msg* msgp, size_t data_len) {
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
    hton_odr_msg(msgp);
    memcpy(buff + sizeof(struct ethhdr), msgp, data_len);

    _DEBUG("crafted frame with proto: %d\n", et->h_proto);
    return sizeof(struct ethhdr) + data_len;
}

/**
*
* |----------------frame----------------|
* |-ethhdr-|-------------data-----------|
* |-ethhdr-|---odr_msg---|---payload----|
*
* Sends data on all ifaces except the one in the except arg.
*
*/
void broadcast(int rawsock, struct hwa_info *hwa_head, struct odr_msg* msgp, int except) {
    unsigned char bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    for(; hwa_head != NULL; hwa_head = hwa_head->hwa_next) {
        if(hwa_head->if_index == except) { /* skip the iface if it is except */
            _DEBUG("skipping iface: %d\n", hwa_head->if_index);
            continue;
        }
        send_on_iface(rawsock, hwa_head, msgp, hwa_head->if_index, bcast);
    }
}

/**
*
* |----------------frame----------------|
* |-ethhdr-|-------------data-----------|
* |-ethhdr-|---odr_msg---|---payload----|
*
* Sends data on only the dst_if interface to dst_mac
* This calls craft_frame and hton_odr_msg()
*/
void send_on_iface(int rawsock, struct hwa_info *hwa_head, struct odr_msg* msgp, int dst_if, unsigned char dst_mac[ETH_ALEN]) {
    char buff[ETH_FRAME_LEN];
    size_t size;
    ssize_t ssize;
    struct sockaddr_ll raw_addr;
    struct in_addr src_addr, dst_addr;
    struct hostent *he;

    for(; hwa_head != NULL; hwa_head = hwa_head->hwa_next) {
        if(hwa_head->if_index == dst_if) {
            break;
        }
    }
    if(hwa_head == NULL) {
        _ERROR("Destination index: %d not found\n", dst_if);
        exit(EXIT_FAILURE);
    }
    _DEBUG("sending on iface: %d\n", dst_if);
    /* all this crap is for a printout !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
    if((0 == inet_aton(msgp->src_ip, &src_addr)) || (0 == inet_aton(msgp->dst_ip, &dst_addr))) {
        _ERROR("One of these are bad: %s or %s\n", msgp->src_ip, msgp->src_ip);
        exit(EXIT_FAILURE);
    }
    printf("ODR at node %s: sending  frame hdr src %s dest ", host_name, host_name);
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n", dst_mac[0], dst_mac[1],
            dst_mac[2], dst_mac[3], dst_mac[4], dst_mac[5]);
    if(NULL == (he = gethostbyaddr(&src_addr, 4, AF_INET))) {
        herror("Serv ipaddr lookup error");
        exit(EXIT_FAILURE);
    }
    printf("\tODR msg  type %d  src %s", msgp->type, he->h_name);
    if(NULL == (he = gethostbyaddr(&dst_addr, 4, AF_INET))) {
        herror("Serv ipaddr lookup error");
        exit(EXIT_FAILURE);
    }
    printf("  dest %s\n", he->h_name);
    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
    size = craft_frame(dst_if, &raw_addr, buff, (unsigned char*)
            hwa_head->if_haddr, dst_mac, msgp,
            sizeof(struct odr_msg) + msgp->len);
    if (size < sizeof(struct ethhdr)) {
        _ERROR("%s\n", "there was an error crafting the packet");
        exit(EXIT_FAILURE);
    }

    ssize = sendto(rawsock, buff, size, 0, (struct sockaddr const *)&raw_addr,
            sizeof(raw_addr));
    if (ssize < (ssize_t) sizeof(struct ethhdr)) {
        _ERROR("%s : %m\n", "sendto()");
        exit(EXIT_FAILURE);
    }
}

void hton_odr_msg(struct odr_msg* msgp) {
    msgp->broadcast_id = htonl(msgp->broadcast_id);
    msgp->dst_port = htons(msgp->dst_port);
    msgp->len = htons(msgp->len);
    msgp->src_port = htons(msgp->src_port);
    msgp->num_hops = ntohs(msgp->num_hops);
}

void ntoh_odr_msg(struct odr_msg* msgp) {
    msgp->broadcast_id = ntohl(msgp->broadcast_id);
    msgp->dst_port     = ntohs(msgp->dst_port);
    msgp->len          = ntohs(msgp->len);
    msgp->src_port     = ntohs(msgp->src_port);
    msgp->num_hops     = ntohs(msgp->num_hops);
}

void hton_sockll(struct sockaddr_ll* addr) {
    addr->sll_family = htons(addr->sll_family);
    addr->sll_hatype = htons(addr->sll_hatype);
    addr->sll_ifindex = htonl((uint32_t)addr->sll_ifindex);
    addr->sll_protocol = htons(addr->sll_protocol);

}

void ntoh_sockll(struct sockaddr_ll* addr) {
    addr->sll_family = ntohs(addr->sll_family);
    addr->sll_hatype = ntohs(addr->sll_hatype);
    addr->sll_ifindex = ntohl((uint32_t)addr->sll_ifindex);
    addr->sll_protocol = ntohs(addr->sll_protocol);

}

/* Can pass NULL for srcip or dstip if not wanted. */
void craft_rreq(struct odr_msg *m, char *srcip, char *dstip, int force_redisc, uint32_t broadcastID) {
    m->type = T_RREQ;
    m->force_redisc = (uint8_t)force_redisc;
    m->broadcast_id = broadcastID;
    if(dstip != NULL) {
        strncpy(m->dst_ip, dstip, INET_ADDRSTRLEN);
    }
    m->dst_port = 0;
    if(srcip != NULL) {
        strncpy(m->src_ip, srcip, INET_ADDRSTRLEN);
    }
    m->src_port = 0;
    m->len = 0;
    m->do_not_rrep = 0;
    m->num_hops = 0;
}

/* Can pass NULL for srcip or dstip if not wanted. */
void craft_rrep(struct odr_msg *m, char *srcip, char *dstip, int force_redisc, int num_hops) {
    m->type = T_RREP;
    m->force_redisc = (uint8_t)force_redisc;
    m->broadcast_id = 0;
    if(dstip != NULL) {
        strncpy(m->dst_ip, dstip, INET_ADDRSTRLEN);
    }
    m->dst_port = 0;
    if(srcip != NULL) {
        strncpy(m->src_ip, srcip, INET_ADDRSTRLEN);
    }
    m->src_port = 0;
    m->len = 0;
    m->do_not_rrep = 1;
    m->num_hops = (uint16_t)num_hops;
}

/**
* Removes eth0 and lo from the list of interfaces.
* Stores the IP of eth0 into the static host_ip
*/
void rm_eth0_lo(struct hwa_info	**hwahead) {
    struct hwa_info *curr, *tofree, *prev;
    int cmp_eth0;
    curr = prev = *hwahead;

    while(curr != NULL) {
        if((0 == (cmp_eth0 = strcmp(curr->if_name, "eth0")))
                || (0 == strcmp(curr->if_name, "lo"))) {
            _DEBUG("Removing interface %s from the interface list.\n", curr->if_name);

            if(cmp_eth0 == 0
                    && curr->ip_alias != IP_ALIAS && curr->ip_addr != NULL) {
                strcpy(host_ip, inet_ntoa(((struct sockaddr_in*)curr->ip_addr)->sin_addr));
                _DEBUG("Found the canonical ip of eth0: %s\n", host_ip);
            }
            tofree = curr;
            curr = curr->hwa_next;
            if(tofree == *hwahead) { /* if we're removing the head then advance the head */
                *hwahead = curr;
            } else {
                prev->hwa_next = curr;
            }
            free(tofree->ip_addr);
            free(tofree);
        } else {
            _DEBUG("Leaving interface %s in the interface list.\n", curr->if_name);
            prev = curr;
            curr = curr->hwa_next;
        }
    }
}


/**
*   Store the message into the queue, to be sent later (upon getting a route).
*
*   malloc()'s space for |-- odr_msg --|--  data  --|  and then puts it at the
*   end of the queue.
*   NOTE: malloc()'s 2 times
*   RETURNS:    0   if the dst IP was not already in the queue
*               -1  if the dst IP was already in the queue.
*/
int queue_store(struct msg_queue *queue, struct odr_msg *m) {
    struct msg_node *new_node, *curr, *prev;
    int cmp;
    size_t real_len;
    if(queue == NULL || m == NULL) {
        _ERROR("%s", "You're msg queue stuff's NULL! Failing!\n");
        exit(EXIT_FAILURE);
    }
    new_node = malloc(sizeof(struct msg_node) + m->len); /* alloc the enclosing msg_node */
    if(new_node == NULL) {
        _ERROR("%s", "malloc() returned NULL! Failing!\n");
        exit(EXIT_FAILURE);
    }
    real_len = sizeof(struct odr_msg) + m->len; /* alloc the inner odr_msg */
    memcpy(&new_node->msg, m, real_len);
    new_node->next = NULL;

    curr = queue->head;
    prev = NULL;
    if(curr == NULL) {        /* case if list is empty */
        queue->head = new_node;
        return 0;
    }
    for( ; curr != NULL; prev = curr, curr = curr->next) {
        cmp = strncmp(m->dst_ip, curr->msg.dst_ip, INET_ADDRSTRLEN);
        if(cmp <= 0){
            new_node->next = curr;
            if(prev == NULL) {
                queue->head = new_node; /* case if before head */
            } else {
                prev->next = new_node;  /* case if after head*/
            }
            return (cmp == 0);
        }
    }
    /* case if last element */
    prev->next = new_node;
    return 0;
}

/**
*   Check if any messages in the queue now have a route in the table.
*   fixme: only pass the route that was just added, not the whole routing table.
*/
void queue_send(struct msg_queue *queue, int rawsock, struct hwa_info *hwa_head, struct tbl_entry *route_tbl) {
    struct msg_node *curr, *prev, *tofree;
    char prev_ip[INET_ADDRSTRLEN] = "\0";
    int route_i = -1;
    if(queue == NULL || route_tbl == NULL) {
        _ERROR("%s", "You're msg queue stuff's NULL! Failing!\n");
        exit(EXIT_FAILURE);
    }
    /**
    *   Since the msgs are ordered based on dst_ip we only look up in the
    *   routing table every time the dst_ip is different than in the previous
    *   msg.
    */
    curr = queue->head;
    prev = NULL;
    while(curr != NULL) {
        if(strncmp(curr->msg.dst_ip, prev_ip, INET_ADDRSTRLEN) != 0) {
            /* ip different so lookup route again */
            route_i = find_route_index(route_tbl, curr->msg.dst_ip);
            strncpy(prev_ip, curr->msg.dst_ip, INET_ADDRSTRLEN);
        }
        /* if way have a route, then ip same so just send */
        if(route_i >= 0) {
            send_on_iface(rawsock, hwa_head, &curr->msg, route_tbl[route_i].iface_index,
                    route_tbl[route_i].mac_next_hop);
            /* now free the msg_node, since it was sent */
            tofree = curr;
            if(prev == NULL) {              /* case if curr is head */
                queue->head = curr->next;
                curr = queue->head;
            } else {                        /* case if curr is not head */
                curr = curr->next;
                prev->next = curr;
            }
            free(tofree);
        } else {
            /* we didn't have a route so just skip this message */
            prev = curr;
            curr = curr->next;
        }
    }
}

/**
* Init the services array to have index 0 as the time server.
* All others are port = -1, with sun_path[0] = '\0', ttl = 0.
*/
void svc_init(struct svc_entry *svcs, size_t len) {
    size_t n = 1;
    if(len < 2) {
        _ERROR("%s", "len too small!\n");
    }
    svcs[0].port = TIME_PORT;
    strcpy(svcs[0].sun_path, TIME_SRV_PATH);
    time(&svcs[0].ttl);                     /* ttl for server never checked  */
    for(; n < SVC_MAX_NUM; n++) {
        svcs[n].port = -1;
        svcs[n].sun_path[0] = '\0';
        svcs[n].ttl = 0;
    }

}


/**
* Adds the svc to the list, or updates the svc's TTL.
* Also loop through the svc list deleting stale svcs.
* Also deletes services whose time_to_live (ttl) has expired.
* RETURNS: the svcs's port number
*              - could be a new port, if service just added
*              - or prev port number if service was updated.
*/
int svc_update(struct svc_entry *svcs, struct sockaddr_un *svc_addr) {
    int n = 0;
    /* updated_port is the min port to add the new entry into, if needed! */
    int updated_port = SVC_MAX_NUM;
    time_t now = time(NULL);        /* only call time() once */

    /* Loop through deleting stale services! If svc_addr is found, then stop */
    for( ; n < SVC_MAX_NUM; n++) {
        if(svcs[n].port < 0) {                          /* entry is empty */
            updated_port = MIN(n, updated_port);
            continue;
        }
        if(svcs[n].ttl <= now && svcs[n].port != TIME_PORT) {   /* entry stale so delete */
            /* entry was stale, deleting */
            /*_DEBUG("DELETE stale service, svcs[%d]: port=%d, path=%s\n", n, svcs[n].port, svcs[n].sun_path);*/
            /*_DEBUG("service was %ld secs old, TTL limit: %d secs\n", (long int)(now - svcs[n].ttl + SVC_TTL), SVC_TTL);*/
            svcs[n].port = -1;
            updated_port = MIN(n, updated_port);
            continue;
        }
        /* this entry wasn't stale */
        if(0 == strncmp(svcs[n].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path))) {
            /* this is the service we're looking for */
            /*_DEBUG("FOUND service, svcs[%d]: port=%d, path=%s\n", n, svcs[n].port, svcs[n].sun_path);*/
            svcs[n].ttl = now + SVC_TTL;
            return svcs[n].port;
        }
    }
    if(updated_port == SVC_MAX_NUM) {
        _ERROR("%s", "The svc array was full!\n");
        exit(EXIT_FAILURE);
    }
    svcs[updated_port].port = updated_port;
    svcs[updated_port].ttl = now + SVC_TTL;
    strncpy(svcs[updated_port].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path));
    svcs[updated_port].sun_path[sizeof(svc_addr->sun_path)-1] = '\0';
    /*_DEBUG("ADD new service, svcs[%d]: port=%d, path=%s\n", updated_port, svcs[updated_port].port, svcs[updated_port].sun_path);*/
    return updated_port;
}


/**
 * Add/Updates a route in the table. Updating is done based on hops.
 *
 * RETURNS: the index the route was added/updated at
 *          -1 if it was not added
 *
 * NOTE: After the call, flags will have 1 if this route was new or more
 *      efficient than the previous route.
 */
int add_route(struct tbl_entry route_table[NUM_NODES], struct odr_msg* msgp,
        struct sockaddr_ll* raw_addr, int staleness, int* eff_flag,
        int rawsock, struct hwa_info* hwa_head, struct msg_queue* queue) {
    int i, is_new_route = 0, exists = 0;
    if(strcmp(msgp->src_ip, host_ip) == 0) {
        _ERROR("%s\n", "trying to add your own ip to the routting table ...");
        exit(EXIT_FAILURE);
    }
    _DEBUG("looking to add ip: %s\n", msgp->src_ip);

    for (i = 0; i < NUM_NODES; ++i) {
        if(route_table[i].ip_dst[0] == 0 || (exists = strncmp(route_table[i].ip_dst, msgp->src_ip, INET_ADDRSTRLEN)) == 0) {
            _DEBUG("adding at index: %d\n", i);
            if(exists) {
                _DEBUG("%s\n", "updating the route");
            } else {
                _DEBUG("%s\n", "adding the route");
            }
            if(route_table[i].ip_dst[0] != 0 && route_table[i].num_hops < msgp->num_hops &&
                    !msgp->force_redisc && (msgp->type == T_RREP || msgp->type == T_RREQ)) {
                _DEBUG("%s\n", "Found a less efficient route but updating bcast id");
                route_table[i].broadcast_id = msgp->broadcast_id;
                *eff_flag = 0;
                return -1;
            }
            /*todo should this also be set if force_redesc?*/
            if(route_table[i].num_hops > msgp->num_hops /*|| msgp.force_redesc*/) {
                *eff_flag = 1;
            }
            if(route_table[i].ip_dst[0] == 0) { /* this route is new */
                *eff_flag = 1;
                is_new_route = 1;
            }
            #ifdef DEBUG
            printf("Old Route\n");
            print_tbl_entry(&(route_table[i]));
            #endif
            memcpy(route_table[i].mac_next_hop, raw_addr->sll_addr, ETH_ALEN);
            route_table[i].iface_index = raw_addr->sll_ifindex;
            route_table[i].num_hops = msgp->num_hops;
            route_table[i].timestamp = time(NULL) + staleness;
            strncpy(route_table[i].ip_dst, msgp->src_ip, 16);
            if(msgp->type == T_RREQ || msgp->type == T_RREP) {  /*only update id if there not data (and not junk)*/
                route_table[i].broadcast_id = msgp->broadcast_id;
            }
            #ifdef DEBUG
            printf("New Route\n");
            print_tbl_entry(&(route_table[i]));
            #endif
            if(is_new_route == 1) {
                /* todo: change from route_table to route_table[i] */
                queue_send(queue, rawsock, hwa_head, route_table);
            }
            return i;
        }
        _DEBUG("index was full : %d\n", i);
    }

    _ERROR("unable to add ip: %s\n", msgp->src_ip);
    exit(EXIT_FAILURE);
    return -1;
}

void print_tbl_entry(struct tbl_entry* entry) {
    printf("-------------------------------\n");
    printf("|mac_next_hop: %02x:%02x:%02x:%02x:%02x:%02x\n",
            entry->mac_next_hop[0], entry->mac_next_hop[1],
            entry->mac_next_hop[2], entry->mac_next_hop[3],
            entry->mac_next_hop[4], entry->mac_next_hop[5]);
    printf("|iface_index : %d\n", entry->iface_index);
    printf("|num_hops    : %d\n", entry->num_hops);
    printf("|timestamp   : %lu\n", (long)entry->timestamp);
    printf("|broadcast_id: %d\n", entry->broadcast_id);
    printf("|ip_dst      : %s\n", entry->ip_dst);
    printf("-------------------------------\n");
}

/**
* removed routs that are stale
*  RETURNS: the index of the matching route
*          -1 if not found or stale
*/
int find_route_index(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN]) {
    int i;
    int deleted;
    if(strcmp(host_ip, ip_dst) == 0) {
        _ERROR("%s\n", "Trying to find your own ip in the routing table ....");
        exit(EXIT_FAILURE);
    }
    time_t now = time(NULL);
    _DEBUG("looking for ip: %s\n", ip_dst);
    for (i = 0; i < NUM_NODES; ++i) {
        if(strncmp(ip_dst, route_table[i].ip_dst, INET_ADDRSTRLEN) == 0) {
            _DEBUG("found match| ip: '%s' index: %d\n", route_table->ip_dst, i);
            if(route_table[i].timestamp  < now) {
                _INFO("%s\n", "it was stale, deleteing it and returning -1\n");
                deleted = delete_route_index(route_table, i);
                if(deleted < 0) {
                    _ERROR("There was an error deleteing index: %d, rtn val: %d\n", i, deleted);
                    exit(EXIT_FAILURE);
                }
                return -1;
            }
            return i;
        }
    }

    _DEBUG("%s\n", "did not find a match");
    return -1;
}

int delete_route_index(struct tbl_entry route_table[NUM_NODES], int index) {
    int at;

    for(at = 0; at < NUM_NODES && route_table[at].ip_dst[0] != 0; ++at);

    if(at < NUM_NODES) {
        memcpy(&route_table[index], &route_table[at], sizeof(struct tbl_entry));
        memset(&route_table[at], 0, sizeof(struct tbl_entry));
        return at;
    }

    return -1;
}

