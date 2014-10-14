#include "util.h"

int int_from_config(FILE* file, char *line, const char* err_str) {
    char *read;
    int rtn;
    read = fgets(line, BUFF_SIZE, file);
    if(read == NULL) {
        perror(err_str);
        return EXIT_FAILURE;
    }
    rtn = atoi(line);
    return rtn;
}
