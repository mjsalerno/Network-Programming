#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <assert.h>

#define DEBUG
#include "debug.h"

/*
Send the first handshake and recv the second.
Return: 0 on success
        -1 on error, with perror() printed
*/
int handshake_first_sec(double p, int serv_fd, struct sockaddr_in *serv_addr);

#endif
