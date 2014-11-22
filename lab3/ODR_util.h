#ifndef ODR_UTIL_H
#define ODR_UTIL_H 1

#include "ODR.h"

struct bid_node {
    uint32_t broadcast_id;
    char src_ip[INET_ADDRSTRLEN];
    struct bid_node* next;
};

char *getvmname(char ip[INET_ADDRSTRLEN]);
void print_odr_msg(struct odr_msg *m);

int add_bid(struct bid_node** head, uint32_t broadcast_id, char src_ip[INET_ADDRSTRLEN]);
int is_dup_msg(struct bid_node* head, struct odr_msg *m);

#endif /* ODR_UTIL_H */
