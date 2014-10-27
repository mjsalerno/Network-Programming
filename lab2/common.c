#include "common.h"
/* Because this is for the config these funcs exit() on failure*/

int int_from_config(FILE* file, const char* err_str) {
	char line[BUFF_SIZE];
    int rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = atoi(line);
    return rtn;
}

double double_from_config(FILE* file, const char* err_str) {
    char line[BUFF_SIZE];
    double rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = atof(line);
    return rtn;
}

/* fill in line */
void str_from_config(FILE* file, char *line, int len, const char* err_str) {
    char *read;
    read = fgets(line, len, file);
    if(read == NULL) {
        if(ferror(file)){
            perror(err_str);
        }
        fprintf(stderr, "%s\n", err_str);
        exit(EXIT_FAILURE);
    }
    while(*read != 0){
        if(*read == '\n'){
            *read = 0;
            break;
        }
        read++;
    }
}

/* pass a bind'ed socket and a pointer to sockaddr_in addr */
void print_sock_name(int sockfd, struct sockaddr_in *addr) {
    int err;
    socklen_t len;
    char ip4_str[INET_ADDRSTRLEN];
    len = sizeof(struct sockaddr_in);
    memset((void *)addr, 0, len);
    err = getsockname(sockfd, (struct sockaddr *)addr, &len);
    if(err < 0) {
        perror("common.print_sock_name.getsockname()");
        exit(EXIT_FAILURE);
    }
    len = sizeof(ip4_str);
    if(NULL == inet_ntop(AF_INET, (void *)&(addr->sin_addr), ip4_str, len)){
        perror("common.print_sock_name.inet_ntop()");
        exit(EXIT_FAILURE);
    }
    printf("%s:%hu\n", ip4_str, ntohs(addr->sin_port));
}

/* pass a connect'ed socket and a pointer to sockaddr_in addr */
void print_sock_peer(int sockfd, struct sockaddr_in *addr) {
    int err;
    socklen_t len;
    char ip4_str[INET_ADDRSTRLEN];
    len = sizeof(struct sockaddr_in);
    memset((void *)addr, 0, len);
    err = getpeername(sockfd, (struct sockaddr *)addr, &len);
    if(err < 0) {
        perror("common.print_sock_peer.getpeername()");
        exit(EXIT_FAILURE);
    }
    len = sizeof(ip4_str);
    if(NULL == inet_ntop(AF_INET, (void *)&(addr->sin_addr), ip4_str, len)){
        perror("common.print_sock_peer.inet_ntop()");
        exit(EXIT_FAILURE);
    }
    printf("%s:%hu\n", ip4_str, ntohs(addr->sin_port));
}

struct iface_info* make_iface_list(void) {
    /*struct iface_info* iface_head;
    struct iface_info* iface_ptr;*/
    struct ifi_info *ifi, *ifihead;
    struct sockaddr *sa;

    ifihead = ifi = get_ifi_info_plus(AF_INET, 0);
    if (ifihead == NULL || ifi == NULL) {
        fprintf(stderr, "get_ifi_info_plus error: no interfaces found\n");
        return NULL;
    }

    for (; ifi != NULL; ifi = ifi->ifi_next) {

        /* todo: check this, has to be up? */
        if (!(ifi->ifi_flags & IFF_UP)) continue;
        if (ifi->ifi_flags & IFF_BROADCAST) continue;
        if (ifi->ifi_flags & IFF_MULTICAST) continue;
        /* if (ifi->ifi_flags & IFF_LOOPBACK) printf("LOOP "); */
        /* if (ifi->ifi_flags & IFF_POINTOPOINT) printf("P2P "); */

        if ((sa = ifi->ifi_addr) == NULL) {
            fprintf(stderr, "Thee IP addr was null\n");
            return NULL;
        }
        printf("  IP addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));

/*=================== cse 533 Assignment 2 modifications ======================*/

        if ((sa = ifi->ifi_ntmaddr) != NULL)
            printf("  network mask: %s\n",
                    sock_ntop_host(sa, sizeof(*sa)));

/*=============================================================================*/

        if ((sa = ifi->ifi_brdaddr) != NULL)
            printf("  broadcast addr: %s\n",
                    sock_ntop_host(sa, sizeof(*sa)));
        if ((sa = ifi->ifi_dstaddr) != NULL)
            printf("  destination addr: %s\n",
                    sock_ntop_host(sa, sizeof(*sa)));
    }
    free_ifi_info_plus(ifihead);
    exit(0);

}

char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen) {
    static char str[128];        /* Unix domain is largest */

    switch (sa->sa_family) {
        case AF_INET: {
            struct sockaddr_in *sin = (struct sockaddr_in *) sa;

            if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
                return (NULL);
            return (str);
        }

#ifdef    IPV6
        case AF_INET6: {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;

            if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
                return (NULL);
            return (str);
        }
#endif
        default:
            snprintf(str, sizeof(str), "sock_ntop_host: unknown AF_xxx: %d, len %d",
                    sa->sa_family, salen);
            return (str);
    }
    return (NULL);
}
