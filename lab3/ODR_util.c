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

/**
* returns -1 if id was smaller then the one here
* returns 0 is the id was the same
* returns 1 if it was added/updated
*/
int add_bid(struct bid_node** head, uint32_t broadcast_id, char src_ip[INET_ADDRSTRLEN]) {
    struct bid_node* ptr;

    for(ptr = *head; ptr != NULL; ptr = ptr->next) {
        if(0 == strcmp(ptr->src_ip, src_ip)) {
            _DEBUG("%s\n", "found maching bid node");
            if(ptr->broadcast_id > broadcast_id) {
                _DEBUG("%s\n", "trying to add broadcast_id that was smaller");
                return -1;
            } else if(ptr->broadcast_id == broadcast_id) {
                _DEBUG("%s\n", "trying to add broadcast_id that was equal");
                return 0;
            }
            ptr->broadcast_id = broadcast_id;
            return 1;
        }
    }

    _DEBUG("%s\n", "did not find a matching bid_node, adding another");
    ptr = malloc(sizeof(struct bid_node));
    if(ptr == NULL) {
        _ERROR("%s\n", "malloc for bid_node failed");
        exit(EXIT_FAILURE);
    }

    strncpy(ptr->src_ip, src_ip, INET_ADDRSTRLEN);
    ptr->broadcast_id = broadcast_id;
    ptr->next = *head;
    *head = ptr;
    return 1;
}


/* RETURN: NULL if not found */
struct bid_node* get_bid(struct bid_node* head, char src_ip[INET_ADDRSTRLEN]) {
    struct bid_node* ptr;

    for(ptr = head; ptr != NULL; ptr = ptr->next) {
        if(0 == strcmp(ptr->src_ip, src_ip)) {
            _DEBUG("%s\n", "found maching bid node");
            break;
        }
    }

    return ptr;
}

/**
*  Only called on RREQs
*  RETURN: 1 if m is a duplicate RREQ
*          0 if it's not
*/
int is_dup_msg(struct bid_node* head, struct odr_msg *m) {
    struct bid_node *old;
    old = get_bid(head, m->src_ip);
    if(old == NULL) {
        /* no prev message from this src_ip, can't be a duplicate */
        return 0;
    }
    if(m->broadcast_id <= old->broadcast_id) { /* it's duplicate */
        return 1;
    }
    return 0;
}


//int update_route_tbl(struct tbl_entry route_table[NUM_NODES], struct odr_msg* msgp, struct sockaddr_ll* raw_addr,
//        int staleness, int* eff_flag, int rawsock, struct hwa_info* hwa_head) {
//    int i, was_empty = 0, is_new_route = 0, err = 0;
//    struct hwa_info* hwa_ptr;
//    if(strcmp(msgp->src_ip, host_ip) == 0) {
//        _ERROR("%s\n", "trying to add your own ip to the routing table ...");
//        abort();
//    }
//    _DEBUG("looking to add/update ip: %s\n", msgp->src_ip);
//
//    for (i = 0; i < NUM_NODES; ++i) {
//        if(route_table[i].ip_dst[0] != '\0') {                                      /* tbl_entry occupied */
//            if(strncmp(route_table[i].ip_dst, msgp->src_ip, INET_ADDRSTRLEN) != 0) {
//                /* occupied with a different route */
//                continue;
//            }
//            was_empty = 0;
//            /* found our slot */
//            break;
//        } else { /* tbl_entry empty */
//            was_empty = 1;
//            /* found our slot */
//            break;
//        }
//        _DEBUG("index was full : %d, checking next index...\n", i);
//    }
//    if(i >= NUM_NODES) {
//        _ERROR("unable to add ip: %s\n", msgp->src_ip);
//        exit(EXIT_FAILURE);
//    }
//    // if was_empty == 1: add the route
//    // else: check stuff
//    /* check if RREQ duplicate */
//    if(msgp->type == T_RREQ) {
//        err = add_bid(&bid_list, msgp->broadcast_id, msgp->src_ip);
//        if(err < 0) {
//            /* this BID is smaller, duplicate RREQ: update only if was_empty */
//        }
//    }
//    return -1;
//}
