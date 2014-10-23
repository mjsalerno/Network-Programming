#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <assert.h>
#include <inttypes.h>

#define DEBUG
#include "debug.h"

struct client_list {
    in_addr_t ip;
    uint16_t port;
    pid_t pid;
    struct client_list* next;
};

int child(char* fname, int par_sock, struct sockaddr_in cli_addr, uint16_t adv_win);
struct client_list* add_client(struct client_list** cl, in_addr_t ip, uint16_t port);
int hand_shake2(int old_sock, struct sockaddr_in old_addr, int new_sock, struct sockaddr_in new_addr, uint16_t adv_win);

#endif
