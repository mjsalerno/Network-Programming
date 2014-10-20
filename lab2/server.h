#ifndef SERVER_H
#define SERVER_H

#include "common.h" 

#define DEBUG
#include "debug.h"

struct client_list {
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    pid_t pid;
    struct client_list* next;
};

int child(int par_sock, struct sockaddr_in cli_addr);
struct client_list* add_client(struct client_list** cl, struct client_list new_cli);

#endif
