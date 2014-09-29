#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define BUFF_SIZE 256
#define TIME_PORT 13
#define ECHO_PORT 14
#define  BACKLOG 1024
#define MAX_FD 256

void time_server(struct sockaddr_in echosrv);
int max(int a, int b);
void my_fd_set(fd_set *fdset, int *arr, int size);

#endif //_SERVER_H_