#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFF_SIZE 256

int int_from_config(FILE* file, char *line, const char* err_str);
