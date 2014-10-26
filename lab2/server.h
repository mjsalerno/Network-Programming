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

int child(char* fname, int par_sock, struct sockaddr_in cliaddr);
struct client_list* add_client(struct client_list** cl, in_addr_t ip, uint16_t port);
int hand_shake2(int par_sock, struct sockaddr_in cliaddr, int child_sock, in_port_t newport);
int send_file(char* fname, int sock, char **wnd);
void free_clients(struct client_list* head);
int remove_client(struct client_list** head, pid_t pid);
void proc_exit(int i);
void send_fin(int sock);
int handle_ack(struct xtcphdr* pkt, char** wnd);

#endif
