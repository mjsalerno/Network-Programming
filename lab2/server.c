#include "server.h"

int main(int argc, const char **argv) {
    const char *path;
    char *read;
    char line[BUFF_SIZE];

    long port;
    long window;


    if(argc < 2) {
        path = "./server.in";
        _DEBUG("%s", "no args: default path is ./server.in\n");
    } else {
        path = argv[1];
    }

    /*Read the config*/
    FILE *file = fopen(path, "r");
    if(file == NULL) {
        perror("could not open server config file");
        return EXIT_FAILURE;
    }

    /*Get the port*/
    read = fgets(line, BUFF_SIZE, file);
    if(read == NULL) {
        perror("there was an error getting the port number");
        return EXIT_FAILURE;
    }
    port = atol(line);
    printf("Port: %ld\n", port);
    if(port < 1) {
        printf("The port can not be less than 1\n");
        return EXIT_FAILURE;
    }
    
    /*Get the window size*/
    read = fgets(line, BUFF_SIZE, file);
    if(read == NULL) {
        perror("there was an error getting the window size number");
        return EXIT_FAILURE;
    }
    window = atol(line);
    printf("Window: %ld\n", window);
    if(port < 1) {
        printf("The window can not be less than 1\n");
        return EXIT_FAILURE;
    }

    printf("config: %s\n", path);

    fclose(file);
    return EXIT_SUCCESS;
}
