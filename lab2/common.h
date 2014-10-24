#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
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
#include "xtcp.h"

#define BUFF_SIZE 512

int int_from_config(FILE* file, const char* err_str);
double double_from_config(FILE* file, const char* err_str);
void str_from_config(FILE* file, char *line, int len, const char* err_str);
void print_sock_name(int sockfd, struct sockaddr_in *addr);
void print_sock_peer(int sockfd, struct sockaddr_in *addr);
int max(int a, int b);

#endif
