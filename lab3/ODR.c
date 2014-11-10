#include <sys/un.h>
#include <unistd.h>
#include "ODR.h"
#include "common.h"

int main(void) {
    int err;
    struct hwa_info *hwahead;
    int unixfd;
    struct sockaddr_un my_addr, cli_addr;
    /*struct svc_entry svcs[MAX_NUM_SVCS];*/
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

    FD_ZERO(&rset);

    for(EVER) {
        FD_SET(unixfd, &rset);
        err = select(unixfd + 1, &rset, NULL, NULL, NULL);
        if(err < 0) {
            perror("ERROR: select()");
            goto cleanup;
        } else if(FD_ISSET(unixfd, &rset)) {
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
