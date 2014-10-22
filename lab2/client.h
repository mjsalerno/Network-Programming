#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <assert.h>

#define DEBUG
#include "debug.h"

/*
Send the first handshake (SYN) and recv the second handshake (SYN-ACK).
Return: 0 on success
        -1 on error, with perror() printed
*/
int handshake_first_sec(double p, int serv_fd, struct sockaddr_in *serv_addr);
/*
Send the third handshake (ACK)
Return: 0 on success
        -1 on error, with perror() printed
*/
int handshake_third(double p, int serv_fd);

#endif
