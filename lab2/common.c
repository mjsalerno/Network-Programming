#include "common.h"
/* Because this is for the config these funcs exit() on failure*/

int int_from_config(FILE* file, const char* err_str) {
	char line[BUFF_SIZE];
    int rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = atoi(line);
    return rtn;
}

float float_from_config(FILE* file, const char* err_str) {
    char line[BUFF_SIZE];
    float rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = (float) atof(line);
    return rtn;
}

/* fill in line */
void str_from_config(FILE* file, char *line, int len, const char* err_str) {
    char *read;
    read = fgets(line, len, file);
    if(read == NULL) {
        if(ferror(file)){
            perror(err_str);
        }
        fprintf(stderr, "%s\n", err_str);
        exit(EXIT_FAILURE);
    }
    while(*read != 0){
        if(*read == '\n'){
            *read = 0;
            break;
        }
        read++;
    }
}
