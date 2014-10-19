#ifndef SERVER_H
#define SERVER_H

#include "common.h" 

#define DEBUG
#include "debug.h"

struct client_list {
    char* ip;
    uint16_t port;
    pid_t pid;
    struct client_list* next;
};

int child(int parent_sock);
struct client_list* add_client(struct client_list** cl, struct client_list new_cli);

#endif
