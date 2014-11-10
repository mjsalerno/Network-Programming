#include <sys/un.h>
#include <unistd.h>
#include <linux/if_arp.h>
#include "ODR.h"

int main(void) {
    int err;
    struct hwa_info *hwahead;
    int unixfd, rawsock;
    struct sockaddr_un local_addr, cli_addr;
    struct sockaddr_ll raw_addr;
    /*struct svc_entry svcs[MAX_NUM_SVCS];*/
    /*socklen_t len;*/

    /* select(2) vars */
    fd_set rset;

    char* buff = malloc(ETH_FRAME_LEN);

    memset(buff, 0, sizeof(ETH_FRAME_LEN));
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));

    if ((hwahead = get_hw_addrs()) == NULL) {       /* get MAC addrs of our interfaces */
        fprintf(stderr, "ERROR: get_hw_addrs()\n");
        exit(EXIT_FAILURE);
    }
    print_hw_addrs(hwahead);


    unixfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if (unixfd < 0) {
        perror("ERROR: socket()");
        free_hwa_info(hwahead);
        exit(EXIT_FAILURE);
    }

    local_addr.sun_family = AF_LOCAL;
    strncpy(local_addr.sun_path, ODR_PATH, sizeof(local_addr.sun_path) - 1);
    unlink(ODR_PATH);

    err = bind(unixfd, (struct sockaddr *) &local_addr, (socklen_t) sizeof(local_addr));
    if (err < 0) {
        perror("ERROR: bind()");
        goto cleanup;
    }

    FD_ZERO(&rset);

    for (EVER) {
        FD_SET(unixfd, &rset);
        err = select(unixfd + 1, &rset, NULL, NULL, NULL);
        if (err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if (FD_ISSET(unixfd, &rset)) {
            /* todo: add to list */
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

/* print the info about all interfaces in the hwa_info linked list */
void print_hw_addrs(struct hwa_info *hwahead) {
    struct hwa_info *hwa_curr;
    struct sockaddr *sa;
    char *ptr, straddrbuf[INET6_ADDRSTRLEN];
    int i, prflag;

    hwa_curr = hwahead;

    printf("\n");
    for (; hwa_curr != NULL; hwa_curr = hwa_curr->hwa_next) {

        printf("%s :%s", hwa_curr->if_name, ((hwa_curr->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");

        if ((sa = hwa_curr->ip_addr) != NULL) {
            switch (sa->sa_family) {
                case AF_INET:
                    inet_ntop(AF_INET, &((struct sockaddr_in *) sa)->sin_addr, straddrbuf, INET6_ADDRSTRLEN);
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
            if (hwa_curr->if_haddr[i] != '\0') {
                prflag = 1;
                break;
            }
        } while (++i < IFHWADDRLEN);

        if (prflag) {                        /* print the MAC address ex) FF:FF:FF:FF:FF:FF */
            printf("         HW addr = ");
            ptr = hwa_curr->if_haddr;
            i = IFHWADDRLEN;
            do {
                printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
            } while (--i > 0);
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
void* craft_frame(int rawsock, struct sockaddr_ll* raw_addr, void* buff, char src_mac[ETH_ALEN], char dst_mac[ETH_ALEN], char* data, size_t data_len) {
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
    }

    et->h_proto = htons(PROTO);
    memcpy(et->h_dest, dst_mac, ETH_ALEN);
    memcpy(et->h_source, src_mac, ETH_ALEN);

    /* copy in the data */
    memcpy(buff + sizeof(struct ethhdr), data, data_len);

    return buff;
}
