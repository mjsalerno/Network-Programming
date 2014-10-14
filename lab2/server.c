#include "server.h"

int main(int argc, const char **argv) {
    const char *path;
    char line[BUFF_SIZE];

    unsigned short port;
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
    port = int_from_config(file, line, "There was an error getting the port number");
    if(port < 1) {
        printf("The port can not be less than 1\n");
        printf("The port can not be less than 1\n");
        return EXIT_FAILURE;
    } 
    printf("Port: %hu\n", port);
    
    /*Get the window size*/
    window = int_from_config(file, line, "There was an error getting the window size number");
    if(window < 1) {
        printf("The window can not be less than 1\n");
        return EXIT_FAILURE;
    }
    printf("Window: %ld\n", window);

    printf("config: %s\n", path);

    fclose(file);
    return EXIT_SUCCESS;
}
