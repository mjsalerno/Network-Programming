#ifndef SERVER_H
#define SERVER_H

#include "common.h" 

#define DEBUG
#include "debug.h"

struct client_list {
    in_addr_t ip;
    uint16_t port;
    pid_t pid;
    struct client_list* next;
};

int child(char* fname, int par_sock, struct sockaddr_in cli_addr);
struct client_list* add_client(struct client_list** cl, in_addr_t ip, uint16_t port);
int hand_shake2(int oldSock, struct sockaddr_in oldAddr, int newSock, struct sockaddr_in newAddr);

#endif
