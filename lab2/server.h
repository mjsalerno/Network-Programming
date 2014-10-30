#ifndef SERVER_H
#define SERVER_H

#include "common.h"

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
int send_file(char* fname, int sock);
void free_clients(struct client_list* head);
int remove_client(struct client_list** head, pid_t pid);
void proc_exit(int i);
void send_fin(int sock);
static void sig_alrm(int signo);
int is_wnd_full();
int is_wnd_empty();
void refresh_timer();
int recv_acks(int sock, int always_block);

#endif
