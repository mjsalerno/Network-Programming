#include "common.h"
/* Because this is for the config these funcs exit() on failure*/

int int_from_config(FILE* file, const char* err_str) {
	char line[BUFF_SIZE];
    int rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = atoi(line);
    return rtn;
}

double double_from_config(FILE* file, const char* err_str) {
    char line[BUFF_SIZE];
    double rtn;
    str_from_config(file, line, sizeof(line), err_str);
    rtn = atof(line);
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

/* pass a bind'ed socket and a pointer to sockaddr_in addr */
void print_sock_name(int sockfd, struct sockaddr_in *addr) {
    int err;
    socklen_t len;
    char ip4_str[INET_ADDRSTRLEN];
    len = sizeof(struct sockaddr_in);
    memset((void *)addr, 0, len);
    err = getsockname(sockfd, (struct sockaddr *)addr, &len);
    if(err < 0) {
        perror("common.print_sock_name.getsockname()");
        exit(EXIT_FAILURE);
    }
    len = sizeof(ip4_str);
    if(NULL == inet_ntop(AF_INET, (void *)&(addr->sin_addr), ip4_str, len)){
        perror("common.print_sock_name.inet_ntop()");
        exit(EXIT_FAILURE);
    }
    printf("%s:%hu\n", ip4_str, ntohs(addr->sin_port));
}

/* pass a connect'ed socket and a pointer to sockaddr_in addr */
void print_sock_peer(int sockfd, struct sockaddr_in *addr) {
    int err;
    socklen_t len;
    char ip4_str[INET_ADDRSTRLEN];
    len = sizeof(struct sockaddr_in);
    memset((void *)addr, 0, len);
    err = getpeername(sockfd, (struct sockaddr *)addr, &len);
    if(err < 0) {
        perror("common.print_sock_peer.getpeername()");
        exit(EXIT_FAILURE);
    }
    len = sizeof(ip4_str);
    if(NULL == inet_ntop(AF_INET, (void *)&(addr->sin_addr), ip4_str, len)){
        perror("common.print_sock_peer.inet_ntop()");
        exit(EXIT_FAILURE);
    }
    printf("%s:%hu\n", ip4_str, ntohs(addr->sin_port));
}

int max(int a, int b) {
    return a > b ? a : b;
}



