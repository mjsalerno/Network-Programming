#ifndef COMMON_H
#define COMMON_H

#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>

#include "xtcp.h"
#include "unpifiplus.h"
#include "unp.h"
#include "config.h"
#include "debug.h"

#define BUFF_SIZE 512
#define EVER ;;

#ifndef MAX
# define MAX(x, y) (((x)>(y))?(x):(y))
#endif

#ifndef MIN
# define MIN(x, y) (((x)<(y))?(x):(y))
#endif

/**
* sockfd
* IP address bound to the socket
* network mask for the IP address
* subnet address (obtained by doing a bit-wise and between the IP address and its network mask)
*/
struct iface_info {
    int sock;
    in_addr_t ip;
    in_addr_t mask;
    in_addr_t subnet;
    struct iface_info* next;
};


int int_from_config(FILE* file, const char* err_str);
double double_from_config(FILE* file, const char* err_str);
void str_from_config(FILE* file, char *line, int len, const char* err_str);
void print_sock_name(int sockfd, struct sockaddr_in *addr);
void print_sock_peer(int sockfd, struct sockaddr_in *addr);

int bind_to_iface_list(struct iface_info* info, uint16_t  port);
struct iface_info* make_iface_list(void);
void free_iface_info(struct iface_info* info);
void print_iface_info(struct iface_info* info);
void print_iface_list(struct iface_info* info);
void print_iface_list_sock_name(struct iface_info* info);
int fd_set_iface_list(struct iface_info* info, fd_set* fdset);
struct iface_info* fd_is_set_iface_list(struct iface_info* info, fd_set* fdset);
struct iface_info* get_iface_from_sock(struct iface_info* info, int sock);
struct iface_info* get_matching_iface_by_ip(struct iface_info* info, in_addr_t ip);
struct iface_info*  get_matching_iface_by_sock(struct iface_info* info, int sock);
void close_sock_iface_info(struct iface_info* info, int sock);


#endif /* COMMON_H */
