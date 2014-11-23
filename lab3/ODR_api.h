#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define BUFF_SIZE 1024
#define EVER ;;
#define ODR_PATH        "/tmp/cse533-8_odr"
#define TIME_SRV_PATH   "/tmp/cse533-8_timesrv"
#define TIME_CLI_PATH   "/tmp/cse533-8_timecli_XXXXXX"
#define TIME_PORT 0
#define PROTO 2691
#define NUM_NODES 10

ssize_t msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port);
ssize_t msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag);

#endif /* COMMON_H */
