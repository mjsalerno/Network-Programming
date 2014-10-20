#include <assert.h>
#include "server.h"

int main(int argc, const char **argv) {
    const char *path;

    FILE *file;
    unsigned short port;
    fd_set fdset;
    long window;
    int pid;
    int err;
    ssize_t n;
/*    int i = 1;*/

/*    struct client_list* cli_list = NULL; */

    int sockfd;
    struct sockaddr_in servaddr, cliaddr, p_serveraddr;/*, p_cliaddr;*/
    socklen_t len;
    char mesg[BUFF_SIZE];

    if(argc < 2) {
        path = "./server.in";
        _DEBUG("%s", "no args: default path is ./server.in\n");
    } else {
        path = argv[1];
    }

    file = fopen(path, "r");

    /*Read the config*/
    if(file == NULL) {
        perror("could not open server config file");
        exit(EXIT_FAILURE);
    }

    /*Get the port*/
    port = (uint16_t) int_from_config(file, "There was an error getting the port number");
    if(port < 1) {
        fprintf(stderr, "The port can not be less than 1\n");
        exit(EXIT_FAILURE);
    } 
    printf("Port: %hu\n", port);
    
    /*Get the window size*/
    window = int_from_config(file, "There was an error getting the window size number");
    if(window < 1) {
        fprintf(stderr, "The window can not be less than 1\n");
        exit(EXIT_FAILURE);
    }
    _DEBUG("Window: %ld\n", window);
    _DEBUG("config: %s\n", path);

    err = fclose(file);
    if(err < 0) {
        perror("server.fclose()");
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    /*servaddr.sin_addr.s_addr = htonl(INADDR_ANY);*/
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

    err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("server.bind()");
        exit(EXIT_FAILURE);
    }

    print_sock_name(sockfd, &p_serveraddr);

    while(1) {

        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);
        err = select(sockfd + 1, &fdset, NULL, NULL, NULL);

        if (err < 0 || !FD_ISSET(sockfd, &fdset)) {
            perror("server.select()");
            exit(EXIT_FAILURE);
        }
        _DEBUG("%s\n", "a client is trying to connect");
        /* TODO: figure out if I this is a client that is already connected */

        /*get filename*/
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, mesg, sizeof(mesg), 0, (struct sockaddr *)&cliaddr, &len);
        if(n < -1) {
            perror("server.recvfrom()");
        }
        mesg[n] = 0;
        _DEBUG("Got filename: %s\n", mesg);

        _DEBUG("%s\n", "server.fork()");
        pid = fork();
        _DEBUG("pid: %d\n", pid);
        if (pid < 0) {
            perror("Could not fork()");
            exit(EXIT_FAILURE);
        }

        /*in child*/
        if (pid == 0) {
            child(sockfd, servaddr);
            /* we should never get here */
            fprintf(stderr, "A child is trying to use the connection select\n");
            assert(0);
        }
    }


    return EXIT_SUCCESS;
}

int testmain(void) {

    struct client_list* list = NULL;
    struct client_list cli;
    struct client_list* p;

    cli.pid = 7;
    cli.next = NULL;
    strncpy(cli.ip, "hello", INET_ADDRSTRLEN);
    cli.port = 9;

    p = add_client(&list, cli);

    printf("---\ncli.pid: %d\ncli.ip: %s\ncli.port: %d\n", cli.pid, cli.ip, cli.port);
    printf("---\np.pid: %d\np.ip: %s\np.port: %d\n", p->pid, p->ip, p->port);
    printf("---\nlst.pid: %d\nlst.ip: %s\nlst.port: %d\n\n", list->pid, list->ip, list->port);

    cli.pid = 10;
    cli.next = NULL;
    strncpy(cli.ip, "bye", INET_ADDRSTRLEN);
    cli.port = 10;

    p = add_client(&list, cli);

    printf("---\ncli.pid: %d\ncli.ip: %s\ncli.port: %d\n", cli.pid, cli.ip, cli.port);
    printf("---\np.pid: %d\np.ip: %s\np.port: %d\n", p->pid, p->ip, p->port);
    printf("---\nlst.pid: %d\nlst.ip: %s\nlst.port: %d\n\n", list->pid, list->ip, list->port);
    return EXIT_SUCCESS;
}

int child(int par_sock, struct sockaddr_in par_addr) {
    struct sockaddr_in servaddr,cliaddr;
    int err;
    int sockfd;
    ssize_t n;
    socklen_t len;
    char msg[BUFF_SIZE];

    _DEBUG("%s\n", "In child");
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    bzero(&cliaddr, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(0);

    /*TODO: pass in ip i want*/
    err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("child.bind()");
        exit(EXIT_FAILURE);
    }

    err = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("child.connect()");
        exit(EXIT_FAILURE);
    }

    /*TODO: send port number*/
    _DEBUG("%s\n", "sending port ...");
    len = sizeof(par_addr);
    n = sendto(par_sock, "MAH POORT IS -4", 16, 0, (struct sockaddr const *)&par_addr, len);
    if (n < 1) {
        perror("child.send(port)");
    }

    /* TODO: err = close(everything);*/
    /* TODO: print out who i am connected to*/

    /*FIXME: not binding on the right thing*/
    n = recvfrom(par_sock, msg, BUFF_SIZE, 0, (struct sockaddr *)&cliaddr,&len);
    if(n < 0) {
        perror("child.recvfrom()");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    msg[n] = 0;
    printf("msg in child: %s\n", msg);
    close(sockfd);
    _DEBUG("%s\n", "Exiting child");
    exit(EXIT_SUCCESS);
}

/* adds a client to the client_list struct if it does not already exist
* returns NULL if the client was already in the list
* returns the pointer to the struct added
*/
struct client_list* add_client(struct client_list** cl, struct client_list new_cli) {

    /*first client*/
    if(*cl == NULL) {
        _DEBUG("%s\n", "add_client: first client in the list");
        *cl = malloc(sizeof(struct client_list));
        if(*cl == NULL) {
            perror("add_client.malloc()");
        }

        (*cl)->port = new_cli.port;
        strncpy(new_cli.ip, (*cl)->ip, INET_ADDRSTRLEN);
        (*cl)->next = NULL;
        (*cl)->pid = -1;
        return *cl;
    } else {
        /*not the first client*/
        struct client_list* p = *cl;
        struct client_list* prev = NULL;

        do {

            if(strcmp(p->ip, new_cli.ip) && p->port == new_cli.port) {
                /*found dup*/
                _DEBUG("%s\n", "found duplicate client, not adding");
                return NULL;
            }
            prev = p;
            p = p->next;

        } while(p != NULL);

        _DEBUG("%s\n", "add_client: adding client to the list");
        p = malloc(sizeof(struct client_list));
        p->port = new_cli.port;
        strncpy(new_cli.ip, p->ip, INET_ADDRSTRLEN);
        p->next = NULL;
        p->pid = -1;
        prev->next = p;
        return p;
    }

}
