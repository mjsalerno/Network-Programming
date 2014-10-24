#include <time.h>
#include "server.h"

struct client_list* cliList;
uint32_t seq;
uint32_t ack;
uint16_t advwin;

int main(int argc, const char **argv) {
    const char *path;

    FILE *file;
    unsigned short port;
    fd_set fdset;
    uint16_t window;
    int pid;
    int err;
    ssize_t n;
    struct client_list* newCli;

    int sockfd;
    struct sockaddr_in servaddr, cliaddr, p_serveraddr;/*, p_cliaddr;*/
    socklen_t len;
    char pkt[BUFF_SIZE];

    cliList = NULL;

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
    window = (uint16_t)int_from_config(file, "There was an error getting the window size number");
    if(window < 1) {
        fprintf(stderr, "The window can not be less than 1\n");
        exit(EXIT_FAILURE);
    }
    _DEBUG("Window: %hu\n", window);
    _DEBUG("config: %s\n", path);

    err = fclose(file);
    if(err < 0) {
        perror("server.fclose()");
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);*/

    err = bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0) {
        perror("server.bind()");
        exit(EXIT_FAILURE);
    }

    print_sock_name(sockfd, &p_serveraddr);

    for(EVER) {

        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);
        err = select(sockfd + 1, &fdset, NULL, NULL, NULL);

        if (err < 0 || !FD_ISSET(sockfd, &fdset)) {
            perror("server.select()");
            exit(EXIT_FAILURE);
        }
        _DEBUG("%s\n", "a client is trying to connect");

        /*get filename*/
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, pkt, sizeof(pkt), 0, (struct sockaddr *)&cliaddr, &len);
        if(n < -1) {
            perror("server.recvfrom()");
        }

        newCli = add_client(&cliList, cliaddr.sin_addr.s_addr, cliaddr.sin_port);
        if(newCli == NULL) {
            _DEBUG("%s\n", "Server: dup clinet, not doing anything");
            continue;
        }
        ntohpkt((struct xtcphdr*)pkt);

        ack = ((struct xtcphdr*)pkt)->seq;

        pkt[n] = 0;
        if(((struct xtcphdr*)pkt)->flags != SYN) {
            printf("server.hs1(): client did not send SYN: %hu\n", ((struct xtcphdr*)pkt)->flags);
            continue;
        }
        print_xtxphdr((struct xtcphdr*) pkt);
        _DEBUG("Got filename: %s\n", pkt + DATA_OFFSET);

        _DEBUG("%s\n", "server.fork()");
        pid = fork();
        if (pid < 0) {
            perror("Could not fork()");
            exit(EXIT_FAILURE);
        }
        _DEBUG("pid: %d\n", pid);

        /*in child*/
        if (pid == 0) {
            child(pkt + DATA_OFFSET, sockfd, cliaddr, window);
            /* we should never get here */
            fprintf(stderr, "A child is trying to use the connection select\n");
            assert(0);
        }
    }


    return EXIT_SUCCESS;
}

int testmain(void) {

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
    return EXIT_SUCCESS;
}

int child(char* fname, int par_sock, struct sockaddr_in cliaddr, uint16_t adv_win) {
    struct sockaddr_in childaddr;
    int err;
    int child_sock;
    socklen_t len;

    _DEBUG("%s\n", "In child");
    _DEBUG("child.filename: %s\n", fname);
    child_sock = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&childaddr, sizeof(childaddr));

    childaddr.sin_family = AF_INET;
    childaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    childaddr.sin_port = htons(0);

    /*TODO: pass in ip i want*/
    err = bind(child_sock, (struct sockaddr *) &childaddr, sizeof(childaddr));
    if (err < 0) {
        perror("child.bind()");
        free_clients(cliList);
        close(child_sock);
        exit(EXIT_FAILURE);
    }

    err = connect(child_sock, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
    if (err < 0) {
        perror("child.connect()");
        free_clients(cliList);
        close(child_sock);
        exit(EXIT_FAILURE);
    }

    len = sizeof(childaddr);
    err = getsockname(child_sock, (struct sockaddr *)&childaddr, &len);
    if(err < 0) {
        perror("child.getsockname()");
        free_clients(cliList);
        close(child_sock);
        exit(EXIT_FAILURE);
    }
    /*_DEBUG("port in network format: %hu\n", childaddr.sin_port);*/

    printf("child bound to: ");
    print_sock_name(child_sock, &childaddr);
    printf("\nchild connected to: ");
    print_sock_peer(child_sock, &cliaddr);
    printf("\n");

    _DEBUG("%s\n", "doing hs2 ...");
    err = hand_shake2(par_sock, cliaddr, child_sock, childaddr.sin_port, adv_win);
    if(err != EXIT_SUCCESS) {
        printf("There was an error with the handshake");
        free_clients(cliList);
        close(child_sock);
        exit(EXIT_FAILURE);
    }

    /* TODO: err = close(everything);*/
    /* print out connection extablished */
    printf("connection established from child at -- ");
    print_sock_name(child_sock, &childaddr);
    printf("connection established to client at  -- ");
    print_sock_peer(child_sock, &cliaddr);

    _DEBUG("%s\n", "Sending file ...");
    send_file(fname, child_sock);

    _DEBUG("%s\n", "sending FIN");
    srvsend(child_sock, ++seq, ack, FIN, adv_win, NULL, 0);

    _DEBUG("%s\n", "Exiting child");
    close(child_sock);
    free_clients(cliList);
    exit(EXIT_SUCCESS);
}

/* adds a client to the client_list struct if it does not already exist
* returns NULL if the client was already in the list
* returns the pointer to the struct added
*/
struct client_list* add_client(struct client_list** cl, in_addr_t ip, uint16_t port) {

    /*first client*/
    if(*cl == NULL) {
        _DEBUG("%s\n", "add_client: first client in the list");
        *cl = malloc(sizeof(struct client_list));
        if(*cl == NULL) {
            perror("add_client.malloc()");
        }

        (*cl)->port = port;
        (*cl)->ip = ip;
        (*cl)->next = NULL;
        (*cl)->pid = -1;
        return *cl;
    } else {
        /*not the first client*/
        struct client_list* p = *cl;
        struct client_list* prev = NULL;

        do {

            if((*cl)->ip == ip && (*cl)->port == port) {
                /*found dup*/
                _DEBUG("%s\n", "found duplicate client, not adding");
                return NULL;
            }
            prev = p;
            p = p->next;

        } while(p != NULL);

        _DEBUG("%s\n", "add_client: adding client to the list");
        p = malloc(sizeof(struct client_list));
        p->port = port;
        p->ip = ip;
        p->next = NULL;
        p->pid = -1;
        prev->next = p;
        return p;
    }

}

int hand_shake2(int par_sock, struct sockaddr_in cliaddr, int child_sock, in_port_t newport, uint16_t adv_win) {
    ssize_t n;
    socklen_t len;
    char pktbuf[BUFF_SIZE];
    struct xtcphdr* hdr = malloc(sizeof(struct xtcphdr) + 2);
    uint16_t flags = 0;
    fd_set rset;
    int err;
    int count = 0;
    struct timeval timer;


    srand48((unsigned int)time(NULL));
    seq = (u_int32_t)lrand48();

    flags = SYN|ACK;
    make_pkt(hdr, ++seq, ++ack, flags, adv_win, &newport, 2);
    printf("sent hs2: ");
    print_xtxphdr(hdr);
    htonpkt(hdr);

    len = sizeof(struct sockaddr_in);
    n = sendto(par_sock, hdr, sizeof(struct xtcphdr) + 2, 0, (struct sockaddr const *)&cliaddr, len);
    if (n < 1) {
        perror("hs2.send(port)");
        free(hdr);
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "waiting to get reply on new child port ...");
    /*todo: use timer*/

    timer.tv_sec = 2;
    timer.tv_usec = 0;

    FD_ZERO(&rset);
    FD_SET(child_sock, &rset);
    err = select(child_sock + 1, &rset, NULL, NULL, &timer);
    if(err < 0) {
        perror("server.hs2()");
        free(hdr);
        exit(EXIT_FAILURE);
    }

    _DEBUG("select(): %d\n", err);

    while(!FD_ISSET(child_sock, &rset) && count < 4) {
        count++;
        _DEBUG("%s\n", "hs2 time out, re-sending over both sockets ...");

        len = sizeof(cliaddr);
        n = sendto(par_sock, hdr, sizeof(struct xtcphdr) + 2, 0, (struct sockaddr const *)&cliaddr, len);
        if (n < 1) {
            perror("hs2.send(port)");
            free(hdr);
            exit(EXIT_FAILURE);
        }
        /* child_sock connected to new_addr? */
        n = send(child_sock, hdr, sizeof(struct xtcphdr) + 2, 0);
        if (n < 1) {
            perror("hs2.send(port)");
            free(hdr);
            exit(EXIT_FAILURE);
        }
        _DEBUG("%s\n", "waiting to get reply on new child port ...");

        timer.tv_sec = 2;
        timer.tv_usec = 0;
        FD_ZERO(&rset);
        FD_SET(child_sock, &rset);
        err = select(child_sock + 1, &rset, NULL, NULL, &timer);
        if(err < 0) {
            perror("hs2.retry()");
            free(hdr);
            exit(EXIT_FAILURE);
        }
    }
    free(hdr);

    /* will happen when breaking out of while above */
    if(!FD_ISSET(child_sock, &rset)) {
        printf("The handshake is broken, exiting child\n");
        exit(EXIT_FAILURE);
    }

    n = recv(child_sock, pktbuf, sizeof(pktbuf), 0);
    if(n < 0) {
        perror("hs2.recvfrom()");
        close(child_sock);
        exit(EXIT_FAILURE);
    }
    ntohpkt((struct xtcphdr*) pktbuf);
    print_xtxphdr((struct xtcphdr*) pktbuf);
    if(((struct xtcphdr*) pktbuf)->flags != ACK
            || ((struct xtcphdr*) pktbuf)->ack_seq != (seq + 1)
            || ((struct xtcphdr*) pktbuf)->seq != ack) {
        printf("client's flags||ack||seq are wrong, handshake broken\n");
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "got last hand shake from client");

    return EXIT_SUCCESS;
}


int send_file(char* fname, int sock) {
    FILE *file;
    char* data[MAX_DATA_SIZE] = {0};
    size_t n;
    int err;

    file = fopen(fname, "r");
    if(file == NULL) {
        perror("server.send_file");
        exit(EXIT_FAILURE);
    }

    for(EVER) {
        n = fread(data, 1, sizeof(data), file);
        _DEBUG("send_file.fread(%lu)", (unsigned long)n);
        if (ferror(file)) {
            printf("server.send_file(): There was an error reading the file\n");
            clearerr(file);
            fclose(file);
            exit(EXIT_FAILURE);
        } else {
            _DEBUG("sending %lu bytes of file\n", (unsigned long)n);
            err = srvsend(sock, ++seq, ack, 0, advwin, data, n);
            if(err < 0) {
                printf("server.send_file(): there was an error sending the file\n");
            } else if (feof(file)) {
                printf("The file has finished uploading ...\n");
                break;
            }
        }
    }

    fclose(file);

    return EXIT_SUCCESS;
}
