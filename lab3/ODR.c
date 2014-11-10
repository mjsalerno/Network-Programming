#include <sys/un.h>
#include <unistd.h>
#include "ODR.h"
#include "common.h"
#include "debug.h"

static char host_ip[INET_ADDRSTRLEN];

int main(void) {
    int err;
    ssize_t n;
    struct hwa_info *hwahead;
    int unixfd;
    struct sockaddr_un my_addr, cli_addr;
    socklen_t len;
    struct svc_entry svcs[MAX_NUM_SVCS];
    int nxt_port = 1;  /* the next lowest port number to use */
    char buf_svc_mesg[MAX_MESG_SIZE];  /* buffer to Mesgs from services */
    struct Mesg *svc_mesg;  /* to cast buf_svc_mesg */
    /*socklen_t len;*/

    /* select(2) vars */
    fd_set rset;

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


    unixfd = socket(AF_LOCAL, SOCK_DGRAM, 0);   /* create local socket */
    if(unixfd < 0) {
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
            }

            if(!svc_contains_path(svcs, &cli_addr)) {   /* if I don't know about them */
                svc_add(svcs, &nxt_port, &cli_addr);    /* then I add them */
            } else {
                /*fixme: this else occurs on servers? */
                _ERROR("%s", "A service was already in the path, maybe it's a server?\n");
            }

            svc_mesg = (struct Mesg*) buf_svc_mesg;
            if(0 == strcmp(svc_mesg->ip, host_ip)) {
                /* the destination IP of this svc's mesg is local */
                _DEBUG("%s", "GOT svc msg with local dest IP: %s\n", svc_mesg->ip);
            } else {
                /* todo: the dest IP is non-local */
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

    for(; n < MAX_NUM_SVCS; n++) {
        svcs[n].port = -1;
        svcs[n].port = '\0';
    }

}

/**
* Always adds to the next lowest port number (lowest index).
* Then finds the next lowest index which is un-occupied.
* RETURNS: stores next port into nxt_port
*/
void svc_add(struct svc_entry *svcs, int *nxt_port, struct sockaddr_un *svc_addr) {
    int n = *nxt_port;
    strncpy(svcs[n].sun_path, svc_addr->sun_path, sizeof(svc_addr->sun_path) - 1);
    svcs[n].port = n;

    while(++n < MAX_NUM_SVCS) {
        if(svcs[n].port == -1) {
            *nxt_port = n;
            return;
        }
    }

    _SPEC("%s", "The svc array is full!\n");
}

/**
* Always adds to the next lowest port number (lowest index).
* If rm_port was lower than the curr nxt_port, then update nxt_port.
*/
void svc_rm(struct svc_entry* array, int *nxt_port, int rm_port) {
    array[rm_port].port = -1;
    array[rm_port].sun_path[0] = '\0';
    if(rm_port < *nxt_port) {
        *nxt_port = rm_port;
    }
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
    for(; n < MAX_NUM_SVCS; ++n) {
        if(0 == strncmp(new_path, svcs[n].sun_path, sizeof(svc_addr->sun_path))) {
            return 1;
        }
    }
    return 0;
}

