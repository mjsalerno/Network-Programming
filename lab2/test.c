#include <stdlib.h>
#include <stdio.h>
#include "server.h"

int main(void) {

    struct client_list* list = NULL;
    struct client_list* p;

    p = add_client(&list, 1, 10);

    printf("---\nip: %d\ncli.port: %d\n", 1, 10);
    printf("---\np.pid: %d\np.ip: %d\np.port: %d\n", p->pid, p->ip, p->port);
    printf("---\nlst.pid: %d\nlst.ip: %d\nlst.port: %d\n\n", list->pid, list->ip, list->port);

    p = add_client(&list, -1, 11);

    printf("---\ncli.ip: %d\ncli.port: %d\n", -1, 11);
    printf("---\np.pid: %d\np.ip: %d\np.port: %d\n", p->pid, p->ip, p->port);
    printf("---\nlst.pid: %d\nlst.ip: %d\nlst.port: %d\n\n", list->pid, list->ip, list->port);

    free_clients(list);
    return EXIT_SUCCESS;
}



int mmain(void) {
    /* clientlist_test(); */
    int err = 0;
    char** wnd = malloc(sizeof(char*) * 3);
    bzero(wnd, sizeof(wnd));
    advwin = 3;
    basewin = 0;

    wnd[0] = 0;
    wnd[1] = 0;
    wnd[2] = 0;

    err = add_to_wnd(0, "hello", (const char **)wnd);
    if(err < 0) {
        _DEBUG("%s\n", "ERR");
        free(wnd);
        return -1;
    }

    err = add_to_wnd(1, "hello", (const char **)wnd);
    if(err < 0) {
        _DEBUG("%s\n", "ERR");
        free(wnd);
        return -1;
    }

    err = add_to_wnd(2, "hello", (const char **)wnd);
    if(err < 0) {
        _DEBUG("%s\n", "ERR");
        free(wnd);
        return -1;
    }

    _DEBUG("%s\n", "build was fine");
    return 1;
}