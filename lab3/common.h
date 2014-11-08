#ifndef COMMON_h

ssize_t void msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port);
ssize_t void msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag);