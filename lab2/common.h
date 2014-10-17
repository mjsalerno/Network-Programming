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
#include <unistd.h>

#define BUFF_SIZE 256

int int_from_config(FILE* file, const char* err_str);
float float_from_config(FILE* file, const char* err_str);
void str_from_config(FILE* file, char *line, int len, const char* err_str);

#endif
