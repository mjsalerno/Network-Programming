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

#endif
