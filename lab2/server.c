#include "server.h"

int main(int argc, const char **argv) {
    const char *path;

    if(argc < 2) {
        path = "./server.in";
        _DEBUG("%s", "no args: default path is ./server.in\n");
    } else {
        path = argv[1];
    }

    printf("%s\n", path);

    return EXIT_SUCCESS;
}
