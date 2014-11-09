#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/socket.h>

#define BUFF_SIZE 1024
#define EVER ;;
#define KNOWN_PATH "./path"

ssize_t msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port);
ssize_t msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag);

#endif /* COMMON_H */