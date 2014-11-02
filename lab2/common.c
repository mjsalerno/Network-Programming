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
    int was_null = 0;

    if(addr == NULL) {
        _DEBUG("%s\n", "was passed a NULL addr, making my own");
        addr = malloc(sizeof(struct sockaddr_in));
        was_null = 1;
    }

    len = sizeof(struct sockaddr_in);
    memset((void *)addr, 0, len);
    err = getsockname(sockfd, (struct sockaddr *)addr, &len);
    if(err < 0) {
        _DEBUG("socket: %d addr: %p\n", sockfd, (void*)addr);
        perror("common.print_sock_name.getsockname()");
        exit(EXIT_FAILURE);
    }
    len = sizeof(ip4_str);
    if(NULL == inet_ntop(AF_INET, (void *)&(addr->sin_addr), ip4_str, len)){
        perror("common.print_sock_name.inet_ntop()");
        exit(EXIT_FAILURE);
    }
    printf("%s:%hu\n", ip4_str, ntohs(addr->sin_port));

    if(was_null) {
        _DEBUG("%s\n", "freeing my addr");
        free(addr);
        addr = NULL;
    }
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

/* creates the iface_list usinf steve iffy code */
struct iface_info* make_iface_list(void) {
    struct iface_info* iface_head;
    struct iface_info* iface_ptr;
    struct ifi_info *ifi, *ifihead;
    struct sockaddr *sa;

    iface_head = NULL;
    iface_ptr = NULL;

    ifihead = ifi = get_ifi_info_plus(AF_INET, 0);
    if (ifihead == NULL || ifi == NULL) {
        fprintf(stderr, "get_ifi_info_plus error: no interfaces found\n");
        return NULL;
    }

    for (; ifi != NULL; ifi = ifi->ifi_next) {

        /* todo: check this, has to be up? */
        /*if (!(ifi->ifi_flags & IFF_UP)) continue;
        if (ifi->ifi_flags & IFF_BROADCAST) continue;
        if (ifi->ifi_flags & IFF_MULTICAST) continue;*/
        /* if (ifi->ifi_flags & IFF_LOOPBACK) printf("LOOP "); */
        /* if (ifi->ifi_flags & IFF_POINTOPOINT) printf("P2P "); */

        if(iface_head == NULL) {
            iface_head = malloc(sizeof(struct iface_info));
            iface_ptr = iface_head;
            iface_head->next = NULL;
        } else {
            iface_ptr->next = malloc(sizeof(struct iface_info));
            iface_ptr = iface_ptr->next;
            iface_ptr->next = NULL;
        }

        if ((sa = ifi->ifi_addr) == NULL) {
            _DEBUG("%s\n", "Thee IP addr was null");
            free_iface_info(iface_head);
            return NULL;
        }
        _DEBUG("IP addr: %s\n", sock_ntop_host(sa, sizeof(*sa)));
        iface_ptr->ip = ((struct sockaddr_in*)sa)->sin_addr.s_addr;

/*=================== cse 533 Assignment 2 modifications ======================*/

        if ((sa = ifi->ifi_ntmaddr) == NULL) {
            _DEBUG("%s\n", "can't find the network mask");
            free_iface_info(iface_head);
            return NULL;
        }
        _DEBUG("network mask: %s\n", sock_ntop_host(sa, sizeof(*sa)));
        iface_ptr->mask = ((struct sockaddr_in*)sa)->sin_addr.s_addr;

/*=============================================================================*/

        iface_ptr->sock = 0;
        /* a bit-wise and between the IP address and its network mask */
        iface_ptr->subnet = iface_ptr->ip & iface_ptr->mask;
    }
    free_ifi_info_plus(ifihead);
    return iface_head;

}

/* frees the iface_list and links, closes sockets, make sockets 0 if you already closed it */
void free_iface_info(struct iface_info* info) {
    if(info == NULL) return;

    if(info->next != NULL)
        free_iface_info(info->next);

    if(info->sock > 0)
        close(info->sock);

    free(info);

}

/* prints one iface_info struct to stdout */
void print_iface_info(struct iface_info* info) {
    struct in_addr addr;

    printf("==========================\n");
    addr.s_addr = info->ip;
    printf("|IP:     %15s |\n", inet_ntoa(addr));
    addr.s_addr = info->mask;
    printf("|Mask:   %15s |\n", inet_ntoa(addr));
    addr.s_addr = info->subnet;
    printf("|Subnet: %15s |\n", inet_ntoa(addr));
    printf("|Socket: %15d |\n", info->sock);
    printf("|Next:   %15p |\n", (void *)info->next);
    printf("|This:   %15p |\n", (void *)info);
    printf("==========================\n");

}

/* prints the whole iface_list to stdout */
void print_iface_list(struct iface_info* info) {
    struct iface_info* ptr;
    ptr = info;

    for(; ptr != NULL; ptr = ptr->next) {
        print_iface_info(ptr);
    }

}

/* binds all of the iface_info struct in the given list */
int bind_to_iface_list(struct iface_info* info, uint16_t  port) {
    struct iface_info* ptr;
    struct sockaddr_in servaddr;

    int sockfd, err;
    err = 0;
    ptr = info;

    for(; ptr != NULL; ptr = ptr->next) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd < 1) {
            perror("bind_to_iface_list()");
            exit(EXIT_FAILURE);
        }
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr.s_addr = ptr->ip;
        ptr->sock = sockfd;

        err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
        if (err < 0) {
            fprintf(stderr, "bind_to_iface_list.bind(%d", err);
            perror(")");
            break;
        }
    }

    return err;
}

/* sets all of the fds in the fdset that are in the iface_list also returns the max fd*/
int fd_set_iface_list(struct iface_info* info, fd_set* fdset) {
    struct iface_info* ptr;
    int cur_sock;
    int max = -100;
    ptr = info;

    FD_ZERO(fdset);

    for(; ptr != NULL; ptr = ptr->next) {
        cur_sock = ptr->sock;

        if(cur_sock < 1) {
            _DEBUG("found an invalid socket in the iface struct: %d", cur_sock);
            return cur_sock;
        }

        FD_SET(cur_sock, fdset);
        max = MAX(max, cur_sock);
    }

    return max;

}

/* returns the pointer to the set iface_info that is set in the fdset */
struct iface_info* fd_is_set_iface_list(struct iface_info* info, fd_set* fdset) {
    struct iface_info* ptr;
    ptr = info;

    for (; ptr != NULL; ptr = ptr->next) {
        if(FD_ISSET(ptr->sock, fdset)) {
            return ptr;
        }
    }

    return NULL;
}

struct iface_info* get_iface_from_sock(struct iface_info* info, int sock) {
    struct  iface_info* ptr;
    ptr = info;

    for(; ptr != NULL; ptr = ptr->next) {
        if(ptr->sock == sock)
            break;
    }

    if(ptr == NULL) {
        _DEBUG("%s\n", "Could not find iface with that socket");
    }

    return ptr;
}

/* calls print_sock_name on all of the iface_info structs */
void print_iface_list_sock_name(struct iface_info* info) {
    struct iface_info* ptr;
    ptr = info;

    for(; ptr != NULL; ptr = ptr->next) {
        _DEBUG("passing socket: %d\n", ptr->sock);
        print_sock_name(ptr->sock, NULL);
    }
}

struct iface_info*  get_matching_iface_by_ip(struct iface_info* info, in_addr_t ip) {
    struct iface_info* ptr;
    struct iface_info* long_ptr = NULL;
    in_addr_t long_mask = 0;

    _DEBUG("given: %X\n", ip);

    for(ptr = info; ptr != NULL; ptr = ptr->next) {
        _DEBUG("lookign at ip: %X\n", ptr->ip);
        _DEBUG("lookign at mask: %X\n", ptr->mask);
        _DEBUG("lookign at subnet: %X\n", ptr->subnet);
        if(((ptr->mask & ip) == ptr->subnet) && (long_mask < ptr->mask)) {
            _DEBUG("%s\n", "this is the longest matching so far");
            long_ptr = ptr;
        }
    }

    if(long_ptr != NULL) {
        _DEBUG("LONGEST MASK: %X\n", long_ptr->mask);
    } else {
        _DEBUG("%s\n", "did not find any local ip, will rout");
    }
    return long_ptr;
}

struct iface_info*  get_matching_iface_by_sock(struct iface_info* info, int sock) {
    struct iface_info* ptr;

    for(ptr = info; ptr != NULL; ptr = ptr->next) {
        if(ptr->sock == sock) {
            _DEBUG("%s\n", "found matching socket");
            return ptr;
        }
    }

    return NULL;
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
