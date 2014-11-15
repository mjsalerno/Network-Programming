#include "ODR.h"
#include "debug.h"

/* fixme: remove */
static char host_ip[INET_ADDRSTRLEN] = "127.0.0.1";
/*static struct tbl_entry route_table[NUM_NODES];*/

int main(void) {
    int err;
    ssize_t n;
    struct hwa_info *hwahead;
    int unixfd, rawsock;
    struct sockaddr_un my_addr, local_addr;
    socklen_t len;
    struct svc_entry svcs[SVC_MAX_NUM];
    char *buf_local_msg;                    /* buffer for local messages */
    struct odr_msg *local_msg;               /* to cast buf_local_msg */
    /*socklen_t len;*/

    /* raw socket vars*/
    /* struct sockaddr_ll raw_addr; */

    /* select(2) vars */
    fd_set rset;

    char* buff = malloc(ETH_FRAME_LEN);
    buf_local_msg = malloc(ODR_MSG_MAX);
    if(buf_local_msg == NULL) {
        _ERROR("%s", "malloc() returned NULL\n");
        exit(EXIT_FAILURE);
    }

    memset(buff, 0, sizeof(ETH_FRAME_LEN));
    memset(&my_addr, 0, sizeof(my_addr));
    memset(&local_addr, 0, sizeof(local_addr));

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


    unixfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if (unixfd < 0) {
        perror("ERROR: socket()");
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }


    my_addr.sun_family = AF_LOCAL;
    strncpy(my_addr.sun_path, ODR_PATH, sizeof(my_addr.sun_path)-1);

    err = bind(unixfd, (struct sockaddr*) &my_addr, (socklen_t)sizeof(my_addr));
    if(err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    svc_init(svcs, SVC_MAX_NUM);   /* ready to accept local msgs */

    FD_ZERO(&rset);
    for(EVER) {
        _DEBUG("%s", "waiting for messages....\n");
        FD_SET(unixfd, &rset);
        err = select(unixfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if(FD_ISSET(unixfd, &rset)) {
            len = sizeof(local_addr);
            n = recvfrom(unixfd, buf_local_msg, SVC_MAX_NUM, 0, (struct sockaddr*)&local_addr, &len);
            if(n < 0) {
                perror("ERROR: recvfrom(unixfd)");
                goto cleanup;
            } else if(n < sizeof(struct odr_msg)) {
                _ERROR("recv'd odr_msg was too short!! n: %d\n", (int)n);
                goto cleanup;
            }
            local_msg = (struct odr_msg*)buf_local_msg;
            _DEBUG("GOT msg from socket: %s, with dest: %s:%d\n",
                    local_addr.sun_path, local_msg->dst_ip, local_msg->dst_port);

            if(0 == strcmp(local_msg->dst_ip, host_ip)) {
                /* the destination IP of this svc's mesg is local */
                _DEBUG("%s", "msg has local dest IP\n");
                err = handle_unix_msg(unixfd, svcs, local_msg, (size_t)n, &local_addr);
                if(err < 0) {
                    goto cleanup;
                }
            } else {
                _DEBUG("FIXME: msg has NON-LOCAL dst IP: %s, host IP: %s\n", local_msg->dst_ip, host_ip);
                /* fixme: dst ip is not local */
            }
            /* note: invalidate ptr to buf_local_msg just to be safe*/
            local_msg = NULL;
        }
    }

    free(buff);
    free(buf_local_msg);
    close(unixfd);
    unlink(ODR_PATH);
    free_hwa_info(hwahead); /* FREE HW list*/
    exit(EXIT_SUCCESS);
cleanup:
    free(buff);
    free(buf_local_msg);
    close(unixfd);
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


    if(m->dst_port < 0 || m->dst_port > SVC_MAX_NUM) {
        _ERROR("service trying to send to a bad port: %d, ignoring...\n", m->dst_port);
        return 0;
    } else if(svcs[m->dst_port].port == -1) {
        /* the destination service doesn't exist, pretend like we sent it */
        _NOTE("no service at dst port: %d, ignoring...\n", m->dst_port);
        return 0;
    }
    dst_addr.sun_family = AF_LOCAL;
    strncpy(dst_addr.sun_path, svcs[m->dst_port].sun_path, sizeof(dst_addr.sun_path));
    dst_addr.sun_path[sizeof(dst_addr.sun_path)-1] = '\0';

    m->src_port = msg_src_port;
    strcpy(m->src_ip, host_ip);

    _DEBUG("Forwarding msg using sendto() to local socket: %s\n", dst_addr.sun_path);
    len = sizeof(dst_addr);
    err = sendto(unixfd, m, mlen, 0, (struct sockaddr*)&dst_addr, len);
    if(err < 0) {
        /* fixme: which sendto() errors should we actually fail on? */
        _ERROR("%s %m. Ignoring error....\n", "sendto():");
        return 0;
    }
    _DEBUG("%s", "succesfully forwarded the msg with sendto()\n");
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
void* craft_frame(int rawsock, int index, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len) {
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
    int n = 0, already_in_svcs = 0;
    /* updated_port is the min port to add the new entry into, if needed! */
    int updated_port = SVC_MAX_NUM;
    time_t now = time(NULL);        /* only call time() once */

    /* Must loop through all to delete the services whose TTL has expired! */
    for( ; n < SVC_MAX_NUM; n++) {
        if(svcs[n].port < 0) {                          /* entry is empty */
            updated_port = MIN(n, updated_port);
            continue;
        }
        if(svcs[n].ttl <= now && svcs[n].port != TIME_PORT) {   /* entry stale so delete */
            /* entry was stale, deleting */
            _DEBUG("DELETE stale service, svcs[%d]: port=%d, path=%s\n", n, svcs[n].port, svcs[n].sun_path);
            printf("service was %ld seconds old, TTL limit: %d\n", (long int)(now - svcs[n].ttl + SVC_TTL), SVC_TTL);
            svcs[n].port = -1;
            updated_port = MIN(n, updated_port);
            continue;
        }
        /* this entry wasn't stale */
        if(0 == strncmp(svcs[n].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path))) {
            /* this is the service we're looking for */
            _DEBUG("FOUND service, svcs[%d]: port=%d, path=%s\n", n, svcs[n].port, svcs[n].sun_path);
            already_in_svcs = 1;
            updated_port = svcs[n].port;
            svcs[n].ttl = now + SVC_TTL;
        }
    }
    if(updated_port == SVC_MAX_NUM) {
        _ERROR("%s", "The svc array was full!\n");
        exit(EXIT_FAILURE);
    }
    if(already_in_svcs) {
        return updated_port;
    }
    svcs[updated_port].port = updated_port;
    svcs[updated_port].ttl = now + SVC_TTL;
    strncpy(svcs[updated_port].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path));
    svcs[updated_port].sun_path[sizeof(svc_addr->sun_path)-1] = '\0';
    _DEBUG("ADD new service, svcs[%d]: port=%d, path=%s\n", updated_port, svcs[updated_port].port, svcs[updated_port].sun_path);
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
int add_route(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN], unsigned char mac_next_hop[ETH_ALEN], int iface_index, int num_hops, int broadcast_id, time_t timestamp) {
    int i, rtn = -1;
    _DEBUG("looking to add ip: %s\n", ip_dst);

    for (i = 0; i < NUM_NODES; ++i) {
        if(route_table[i].ip_dst[0] == 0 || strncmp(route_table[i].ip_dst, ip_dst, INET_ADDRSTRLEN) == 0) {
            _DEBUG("found empty index: %d\n", i);
            memcpy(route_table[i].mac_next_hop, mac_next_hop, ETH_ALEN);
            route_table[i].iface_index = iface_index;
            route_table[i].num_hops = num_hops;
            route_table[i].timestamp = timestamp;
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

/* returns the index of the matching route or -1 if not found*/
int find_route_index(struct tbl_entry route_table[NUM_NODES], char ip_dst[INET_ADDRSTRLEN]) {
    int i;
    _DEBUG("looking for ip: %s\n", ip_dst);
    for (i = 0; i < NUM_NODES; ++i) {
        if(strncmp(ip_dst, route_table[i].ip_dst, 16) == 0) {
            _DEBUG("found match| ip: '%s' index: %d\n", route_table->ip_dst, i);
            return i;
        }
    }

    _DEBUG("%s\n", "did not find a match");
    return -1;
}

