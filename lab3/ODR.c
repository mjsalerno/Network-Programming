#include "ODR.h"
#include "debug.h"

/* fixme: remove */
static char host_ip[INET_ADDRSTRLEN] = "127.0.0.1";
static struct tbl_entry route_table[NUM_NODES];

int main(int argc, char *argv[]) {
    int err;
    ssize_t n;
    struct hwa_info *hwahead;
    int unixsock, rawsock, stdinfd;
    struct sockaddr_un my_addr, local_addr;
    socklen_t len;
    struct svc_entry svcs[SVC_MAX_NUM];
    char *buf_msg;                    /* buffer for local messages */
    struct odr_msg *msgp;               /* to cast buf_local_msg */
    struct odr_msg *out_msg;
    uint32_t broadcastID = 0;

    int staleness;
    /*socklen_t len;*/

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
    memset(&my_addr, 0, sizeof(my_addr));
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&raw_addr, 0, sizeof(raw_addr));

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

    unlink(ODR_PATH); /* unlink before dropping root */

/*    if(setuid(1000) < 0) {
        _ERROR("setuid(%d): %m\n", 1000);
        exit(EXIT_FAILURE);
    }*/


    unixsock = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if (unixsock < 0) {
        perror("ERROR: socket()");
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }


    my_addr.sun_family = AF_LOCAL;
    strncpy(my_addr.sun_path, ODR_PATH, sizeof(my_addr.sun_path)-1);

    err = bind(unixsock, (struct sockaddr*) &my_addr, (socklen_t)sizeof(my_addr));
    if(err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    svc_init(svcs, SVC_MAX_NUM);   /* ready to accept local msgs */

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
            n = recvfrom(unixsock, buf_msg, SVC_MAX_NUM, 0, (struct sockaddr*)&local_addr, &len);
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

            if(0 == strcmp(msgp->dst_ip, host_ip)) {
                /* the destination IP of this svc's mesg is local */
                _DEBUG("%s", "msg has local dest IP\n");
                err = handle_unix_msg(unixsock, svcs, msgp, (size_t)n, &local_addr);
                if(err < 0) {
                    goto cleanup;
                }
            } else {
                _DEBUG("FIXME: msg has NON-LOCAL dst IP: %s, host IP: %s\n", msgp->dst_ip, host_ip);
                /* fixme: dst ip is not local */
                if(msgp->force_redisc) {
                    /* pretend like we don't have a route, send RREQ */
                    memset(out_msg, 0, sizeof(struct odr_msg));
                    craft_rreq(out_msg, host_ip, msgp->dst_ip, 1, broadcastID);
                    broadcastID++;
                    /* send on all ifaces */
                    /* queue the service's msg */
                    /* done ? */

                } else {

                }
            }
            /* note: invalidate ptr to buf_msg just to be safe*/
            msgp = NULL;
        } else if(FD_ISSET(rawsock, &rset)) {   /* something on the raw socket */
            len = sizeof(raw_addr);
            int route_index;
            n = recvfrom(rawsock, buf_msg, SVC_MAX_NUM, 0, (struct sockaddr*)&local_addr, &len);
            if(n < 0) {
                perror("ERROR: recvfrom(rawsock)");
                goto cleanup;
            } else if(n < (ssize_t)sizeof(struct odr_msg)) {
                _ERROR("recv'd odr_msg was too short!! n: %d\n", (int)n);
                goto cleanup;
            }
            msgp = (struct odr_msg*) buf_msg;

            switch(msgp->type) {

                case T_RREQ:
                    route_index = find_route_index(route_table, msgp->src_ip);
                    if(route_index >= 0) { /* we have a route for it*/

                    } else { /* we do not have a route to it */
                        err = add_route(route_table, msgp->src_ip, raw_addr.sll_addr, raw_addr.sll_ifindex, msgp->num_hops, msgp->broadcast_id, staleness);
                        if (err < 0) {
                            _ERROR("%s\n", "There was an error adding the route");
                        }
                    }
                    break;
                case T_RREP:
                    break;
                case T_DATA:
                    break;
                default:
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
    close(unixsock);
    close(rawsock);
    unlink(ODR_PATH);
    free_hwa_info(hwahead); /* FREE HW list*/
    exit(EXIT_SUCCESS);
cleanup:
    free(buff);
    free(buf_msg);
    close(unixsock);
    close(rawsock);
    unlink(ODR_PATH);
    free_hwa_info(hwahead);
    exit(EXIT_FAILURE);
}

/**
* We have recvfrom()'d off of our PF_UNIX socket, the destination is local.
* Update the service list.
*   -Pass to service if local
*   -Pass to rawsock handler if non-local.
*
* NOTE: struct odr_msg *m: m is a pointer to the start of the recv(2) buffer.
*                          - it is mlen bytes long.
*
* RETURNS:  0 on successfully sent
*           -1 on sendto(2) failure, need to cleanup and exit
*/
int handle_unix_msg(int unixfd, struct svc_entry *svcs,
        struct odr_msg *m, size_t mlen, struct sockaddr_un *from_addr) {

    struct sockaddr_un dst_addr = {0};
    int msg_src_port;
    socklen_t len;
    ssize_t err;

    msg_src_port = svc_update(svcs, from_addr);


    if(m->dst_port > SVC_MAX_NUM) {
        _ERROR("service trying to send to a bad port: %d, ignoring...\n", m->dst_port);
        return 0;
    } else if(svcs[m->dst_port].port == -1) {
        /* the destination service doesn't exist, pretend like we sent it */
        _ERROR("no service at dst port: %d, ignoring...\n", m->dst_port);
        return 0;
    }
    dst_addr.sun_family = AF_LOCAL;
    strncpy(dst_addr.sun_path, svcs[m->dst_port].sun_path, sizeof(dst_addr.sun_path));
    dst_addr.sun_path[sizeof(dst_addr.sun_path)-1] = '\0';

    m->src_port = (uint16_t)msg_src_port;
    strcpy(m->src_ip, host_ip);


    len = sizeof(dst_addr);
    err = sendto(unixfd, m, mlen, 0, (struct sockaddr*)&dst_addr, len);
    if(err < 0) {
        /* fixme: which sendto() errors should we actually fail on? */
        _ERROR("%s %m. Ignoring error....\n", "sendto():");
        return 0;
    }
    _DEBUG("Relayed msg: sendto() local sock: %s\n", dst_addr.sun_path);
    return 0;
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
void* craft_frame(int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len) {
    struct ethhdr* et = buff;
    if(data_len > ETH_DATA_LEN) {
        fprintf(stderr, "ERROR: craft_frame(): data_len too big\n");
        exit(EXIT_FAILURE);
    }

    if(raw_addr != NULL) {
        /*prepare sockaddr_ll*/
        memset(raw_addr, 0, sizeof(struct sockaddr_ll));

        /*RAW communication*/
        raw_addr->sll_family = PF_PACKET;
        raw_addr->sll_protocol = htons(PROTO);

        /*todo: index of the network device*/
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

    /* copy in the data */
    /* todo: also copy the ODR hdr */
    memcpy(buff + sizeof(struct ethhdr), data, data_len);

    return buff;
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
}

/**
* Removes eth0 and lo from the list of interfaces.
* Stores the IP of eth0 into the static host_ip
*/
void rm_eth0_lo(struct hwa_info	**hwahead) {
    struct hwa_info *curr, *tofree, *prev;
    curr = prev = *hwahead;

    while(curr != NULL) {
        if(0 == strcmp(curr->if_name, "eth0")) {
            _DEBUG("Removing interface %s from the interface list.\n", curr->if_name);

            if(curr->ip_alias != IP_ALIAS && curr->ip_addr != NULL) {
                strcpy(host_ip, inet_ntoa(((struct sockaddr_in*)curr->ip_addr)->sin_addr));
                _DEBUG("Found the canonical ip of eth0: %s\n", host_ip);
            }
            tofree = curr;
            curr = curr->hwa_next;
            prev->hwa_next = curr;
            if(tofree == *hwahead) { /* if we're removing the head then advance the head */
                *hwahead = curr;
            }
            free(tofree->ip_addr);
            free(tofree);
        } else if(0 == strcmp(curr->if_name, "lo")) {
            _DEBUG("Removing interface %s from the interface list.\n", curr->if_name);

            tofree = curr;
            curr = curr->hwa_next;
            prev->hwa_next = curr;
            if(tofree == *hwahead) { /* if we're removing the head then advance the head */
                *hwahead = curr;
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
 * adds a route to the table
 * If a route witht he same dest ip was found,
 * the entry is replaced without question
 *
 * returns the index it was added to
 * returns -1 if it was not added
 */
int add_route(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN], unsigned char mac_next_hop[ETH_ALEN], int iface_index, uint16_t num_hops, uint32_t broadcast_id, int staleness) {
    int i, rtn = -1;
    _DEBUG("looking to add ip: %s\n", ip_dst);

    for (i = 0; i < NUM_NODES; ++i) {
        if(route_table[i].ip_dst[0] == 0 || strncmp(route_table[i].ip_dst, ip_dst, INET_ADDRSTRLEN) == 0) {
            _DEBUG("found empty index: %d\n", i);
            memcpy(route_table[i].mac_next_hop, mac_next_hop, ETH_ALEN);
            route_table[i].iface_index = iface_index;
            route_table[i].num_hops = num_hops;
            route_table[i].timestamp = time(NULL) + staleness;
            route_table[i].broadcast_id = broadcast_id;
            strncpy(route_table[i].ip_dst, ip_dst, 16);
            rtn = i;
            break;
        }
        _DEBUG("index was full : %d\n", i);
    }

    _DEBUG("unable to add ip: %s\n", ip_dst);
    return rtn;
}

/**
* removed routs that are stale
*  RETURNS: the index of the matching route
*          -1 if not found or stale
*/
int find_route_index(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN]) {
    int i;
    time_t now = time(NULL);
    _DEBUG("looking for ip: %s\n", ip_dst);
    for (i = 0; i < NUM_NODES; ++i) {
        if(strncmp(ip_dst, route_table[i].ip_dst, INET_ADDRSTRLEN) == 0) {
            _DEBUG("found match| ip: '%s' index: %d\n", route_table->ip_dst, i);
            if(route_table[i].timestamp  < now) {
                _DEBUG("%s\n", "it was stale, deleteing it and returning -1\n");
                route_table[i].ip_dst[0] = 0;
                return -1;
            }
            return i;
        }
    }

    _DEBUG("%s\n", "did not find a match");
    return -1;
}

