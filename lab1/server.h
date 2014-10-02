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
#include "const.h"


struct thread_args {
    int connfd;
};

void time_server(struct thread_args *targs);
void echo_server(struct thread_args *targs);

#endif /*_SERVER_H_*/