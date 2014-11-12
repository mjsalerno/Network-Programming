#include <sys/un.h>
#include <unistd.h>
#include <time.h>
#include "ODR.h"
#include "common.h"
#include "debug.h"

static char host_ip[INET_ADDRSTRLEN];

int main(void) {
    int err;
    ssize_t n;
    struct hwa_info *hwahead;
    int unixfd, rawsock;
    struct sockaddr_un my_addr, cli_addr;
    socklen_t len;
    struct svc_entry svcs[SVC_MAX_NUM];
    char buf_svc_mesg[API_MSG_MAX];  /* buffer to Mesgs from services */
    struct api_msg svc_mesg;  /* to cast buf_svc_mesg */
    /*socklen_t len;*/
    
    /* raw socket vars*/
    struct sockaddr_ll raw_addr;
    unsigned char src_mac[6] = {0x00, 0x01, 0x02, 0xFA, 0x70, 0xAA};
    unsigned char dst_mac[6] = {0x00, 0x04, 0x75, 0xC8, 0x28, 0xE5};

    /* select(2) vars */
    fd_set rset;

    char* buff = malloc(ETH_FRAME_LEN);

    memset(buff, 0, sizeof(ETH_FRAME_LEN));
    memset(&my_addr, 0, sizeof(my_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

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
    craft_frame(rawsock, &raw_addr, buff, src_mac, dst_mac, "sup", 4);
    /*fixme ^^*/


    unixfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if (unixfd < 0) {
        perror("ERROR: socket()");
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }


    my_addr.sun_family = AF_LOCAL;
    strncpy(my_addr.sun_path, ODR_PATH, sizeof(my_addr.sun_path)-1);
    unlink(ODR_PATH);

    err = bind(unixfd, (struct sockaddr*) &my_addr, (socklen_t)sizeof(my_addr));
    if(err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    svc_init(svcs, sizeof(svcs));   /* init the service array */
    
    FD_ZERO(&rset);
    for(EVER) {
        FD_SET(unixfd, &rset);
        err = select(unixfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if(FD_ISSET(unixfd, &rset)) {
            /* todo: add to list */
            len = sizeof(cli_addr);
            n = recvfrom(unixfd, buf_svc_mesg, sizeof(buf_svc_mesg), 0, (struct sockaddr*)&cli_addr, &len);
            if(n < 0) {
                perror("ERROR: recvfrom(unixfd)");
                goto cleanup;
            } else if(n < sizeof(struct api_msg)) {
                _ERROR("recv'd api_msg was too short!! n: %d\n", (int)n);
                goto cleanup;
            }
            memcpy(&svc_mesg, buf_svc_mesg, sizeof(svc_mesg));/*align the struct*/

            /* recvd_app_mesg(n, &svc_mesg, &cli_addr); */

            /* svc_add() */


            if(0 == strcmp(svc_mesg.dst_ip, host_ip)) {
                /* the destination IP of this svc's mesg is local */
                _DEBUG("GOT svc msg with local dest IP: %s\n", svc_mesg.dst_ip);
            } else {
                /* fixme: the dest IP is non-local */
            }

            /* todo: where i left off, look at port in svc_mesg or something */

        }

    }


    close(unixfd);
    unlink(ODR_PATH);
    free_hwa_info(hwahead); /* FREE HW list*/
    exit(EXIT_SUCCESS);
cleanup:
    close(unixfd);
    unlink(ODR_PATH);
    free_hwa_info(hwahead);
    exit(EXIT_FAILURE);
}

int recvd_app_mesg(size_t bytes, struct api_msg *m, struct sockaddr_un *from_addr) {
    return -1;
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
void* craft_frame(int rawsock, struct sockaddr_ll* raw_addr, void* buff, unsigned char src_mac[ETH_ALEN], unsigned char dst_mac[ETH_ALEN], char* data, size_t data_len) {
    struct ethhdr* et = buff;
    if(data_len > ETH_DATA_LEN) {
        fprintf(stderr, "ERROR: craft_frame(): data_len too big\n");
        exit(EXIT_FAILURE);
    }

    if(raw_addr != NULL) {
        /*prepare sockaddr_ll*/

        /*RAW communication*/
        raw_addr->sll_family = PF_PACKET;
        raw_addr->sll_protocol = htons(PROTO);

        /*todo: index of the network device*/
        raw_addr->sll_ifindex = 2;

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
            free(tofree);
        } else if(0 == strcmp(curr->if_name, "lo")) {
            _DEBUG("Removing interface %s from the interface list.\n", curr->if_name);

            tofree = curr;
            curr = curr->hwa_next;
            prev->hwa_next = curr;
            if(tofree == *hwahead) { /* if we're removing the head then advance the head */
                *hwahead = curr;
            }
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
* All others are -1, with sun_path[0] = '\0'.
*/
void svc_init(struct svc_entry *svcs, size_t len) {
    size_t n = 1;
    if(len < 2) {
        _ERROR("%s", "len too small!\n");
    }

    svcs[0].port = 0;
    strcpy(svcs[0].sun_path, TIME_SRV_PATH);
    time(&svcs[0].ttl);                     /* ttl for server never checked  */

    for(; n < SVC_MAX_NUM; n++) {
        svcs[n].port = -1;
        svcs[n].port = '\0';
    }

}

/**
* Adds the svc to the list, or updates the svc's TTL.
* Also loop through the svc list deleteing stale svcs.
* RETURNS: port number
*              - could be a new port, if service just added
*              - or prev port number if service was updated.
*/
int svc_add(struct svc_entry *svcs, struct sockaddr_un *svc_addr) {
    int n = 1;
    /* the min port to add the new entry into, if needed! */
    int minport = SVC_MAX_NUM;
    /* let's not recompute the current time over and over*/
    time_t now = time(NULL);

    if(svc_addr->sun_path[0] == '\0') {
        _ERROR("%s", "svc_addr->sun_path is empty!\n");
        exit(EXIT_FAILURE);
    }


    svcs[n].port = n;
    /* delete the services whose TTL has expired! Must loop through. */
    while(++n < SVC_MAX_NUM) {
        if( 0 == strncmp(svcs[n].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path)-1)){
            /* this svc is already in the list! Update TTL */
            svcs[n].ttl = now;
            return svcs[n].port;
        } else if(svcs[n].port != -1){
            /* delete an entry if its old*/
            if((now - svcs[n].ttl) > SVC_TTL) {
                svcs[n].port = -1;
            }
        }
        /* wasn't occupied or was deleted */
        if (minport > n)
            minport = n;
    }
    if(minport == SVC_MAX_NUM) {
        _ERROR("%s", "The svc array is full!\n");
        exit(EXIT_FAILURE);
    }

    svcs[minport].port = minport;
    svcs[minport].ttl = now;
    strncpy(svcs[n].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path)-1);
    return minport;
}

/* 1 if contains a service at the port number, 0 otherwise */
int svc_contains_port(struct svc_entry *svcs, int port) {
    return (svcs[port].port != -1);
}

/* 1 if contains a service at the port number, 0 otherwise */
/* fixme: might not even need this? If we do need, then change to a linked list? */
int svc_contains_path(struct svc_entry *svcs, struct sockaddr_un *svc_addr) {
    int n = 0;
    char *new_path = svc_addr->sun_path;
    for(; n < SVC_MAX_NUM; ++n) {
        if(0 == strncmp(new_path, svcs[n].sun_path, sizeof(svc_addr->sun_path))) {
            return 1;
        }
    }
    return 0;
}

