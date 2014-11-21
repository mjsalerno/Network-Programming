#include "ODR_util.h"
#include "debug.h"

extern char host_ip[INET_ADDRSTRLEN];
extern char host_name[BUFF_SIZE];

char *getvmname(char ip[INET_ADDRSTRLEN]) {
    struct in_addr vmaddr = {0};
    struct hostent *he;
    char *name;
    int i = 0;
    if(strncmp(host_ip, ip, INET_ADDRSTRLEN) == 0) {
        /* for vm9, it conflicts with nplclient29 */
        return host_name;
    }
    if(0 == inet_aton(ip, &vmaddr)) {
        _ERROR("Bad ip: %s\n", ip);
        exit(EXIT_FAILURE);
    }

    if(NULL == (he = gethostbyaddr(&vmaddr, 4, AF_INET))) {
        herror("ERROR gethostbyaddr()");
        exit(EXIT_FAILURE);
    }
    name = he->h_name;
    while(name != NULL && name[0] != 'v' && name[1] != 'm') {
        name = he->h_aliases[i];
        i++;
    }

    return name;
}

void print_odr_msg(struct odr_msg *m) {
    printf("Type: %hhu:  %s:%hu --> ", m->type, getvmname(m->src_ip), m->src_port);
    printf("%s:%hu  BID=%2u, HOPS=%hu, DNR=%hhu, FORCE=%hhu\n", getvmname(m->dst_ip), m->dst_port,
            m->broadcast_id, m->num_hops, m->do_not_rrep, m->force_redisc);
}