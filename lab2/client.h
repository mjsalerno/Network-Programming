#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

#include "debug.h"

/*
Performs handshakes.
Returns -1 on failure with perror() printed
        0 on success
*/
int handshakes(int serv_fd, struct sockaddr_in *serv_addr, char *fname);
int validate_hs2(struct xtcphdr* hdr, int len);

#endif
