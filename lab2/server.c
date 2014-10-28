#include <time.h>
#include "server.h"

struct client_list* cliList;

extern uint32_t seq;
extern uint32_t ack_seq;
extern uint16_t advwin;

static uint32_t start_seq;
static uint16_t cli_wnd;
static struct iface_info* ifaces;

int main(int argc, const char **argv) {
    const char *path;

    FILE *file;
    unsigned short port;
    fd_set fdset;
    int pid;
    int err;
    ssize_t n;
    struct client_list* newCli;

    int sockfd;
    struct sockaddr_in servaddr, cliaddr, p_serveraddr;/*, p_cliaddr;*/
    socklen_t len;
    char pkt[MAX_PKT_SIZE + 1];

    cliList = NULL;
    ifaces = make_iface_list();
    print_iface_list(ifaces);

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
    advwin = (uint16_t)int_from_config(file, "There was an error getting the window size number");
    if(advwin < 1) {
        fprintf(stderr, "The window can not be less than 1\n");
        exit(EXIT_FAILURE);
    }
    _DEBUG("Window: %hu\n", advwin);
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
    if(SIG_ERR == signal(SIGCHLD, proc_exit)) {
        perror("server.signal()");
    }


    for(EVER) {

        FD_ZERO(&fdset);
        FD_SET(sockfd, &fdset);
        err = select(sockfd + 1, &fdset, NULL, NULL, NULL);

        if (err < 0) {
            if(errno == EINTR) {
                _DEBUG("%s\n", "select interupted, continue ...");
                continue;
            }

            perror("server.select()");
            exit(EXIT_FAILURE);
        }
        _DEBUG("%s\n", "a client is trying to connect");


        /*get filename*/
        do {
            len = sizeof(cliaddr);
            _DEBUG("%s\n", "waking up and going into recvfrom");
            n = recvfrom(sockfd, pkt, MAX_PKT_SIZE, 0, (struct sockaddr *) &cliaddr, &len);
            _DEBUG("out of recvfrom n: %d errno: %d\n", (int)n, errno);
            if (n < 0 && errno != EINTR) {
                perror("server.recvfrom()");
                fprintf(stderr, "continuing ...\n");
                continue;
            }
        } while(errno == EINTR && n < 0);

        newCli = add_client(&cliList, cliaddr.sin_addr.s_addr, cliaddr.sin_port);
        if(newCli == NULL) {
            _DEBUG("%s\n", "Server: dup clinet, not doing anything");

            ntohpkt((struct xtcphdr*)pkt);
            printf("GOT: ");
            print_hdr((struct xtcphdr *) pkt);
            continue;
        }
        ntohpkt((struct xtcphdr*)pkt);
        ack_seq = ((struct xtcphdr*)pkt)->seq;

        pkt[n] = 0;
        if(((struct xtcphdr*)pkt)->flags != SYN) {
            printf("server.hs1(): client did not send SYN: %hu\n", ((struct xtcphdr*)pkt)->flags);
            continue;
        }

        /* pick the smallest advwin */
        advwin = ((struct xtcphdr*)pkt)->advwin < advwin ? ((struct xtcphdr*)pkt)->advwin : advwin;
        cli_wnd = advwin;
        _DEBUG("new advwin: %d\n", advwin);

        print_hdr((struct xtcphdr *) pkt);
        _DEBUG("Got filename: %s\n", pkt + DATA_OFFSET);

        _DEBUG("%s\n", "server.fork()");
        pid = fork();
        if (pid < 0) {
            perror("Could not fork()");
            exit(EXIT_FAILURE);
        }
        _DEBUG("pid: %d\n", pid);
        newCli->pid = pid;

        /*in child*/
        if (pid == 0) {
            child(pkt + DATA_OFFSET, sockfd, cliaddr);
            /* we should never get here */
            fprintf(stderr, "A child is trying to use the connection select\n");
            assert(0);
        }
    }


    return EXIT_SUCCESS;
}

int child(char* fname, int par_sock, struct sockaddr_in cliaddr) {
    struct sockaddr_in childaddr;
    int err;
    int child_sock;
    socklen_t len;
    char** wnd;  /* the actual window */

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
        close(par_sock);
        exit(EXIT_FAILURE);
    }

    err = connect(child_sock, (struct sockaddr *) &cliaddr, sizeof(cliaddr));
    if (err < 0) {
        perror("child.connect()");
        free_clients(cliList);
        close(child_sock);
        close(par_sock);
        exit(EXIT_FAILURE);
    }

    len = sizeof(childaddr);
    err = getsockname(child_sock, (struct sockaddr *)&childaddr, &len);
    if(err < 0) {
        perror("child.getsockname()");
        free_clients(cliList);
        close(child_sock);
        close(par_sock);
        exit(EXIT_FAILURE);
    }

    printf("child bound to: ");
    print_sock_name(child_sock, &childaddr);
    printf("\nchild connected to: ");
    print_sock_peer(child_sock, &cliaddr);
    printf("\n");

    _DEBUG("%s\n", "doing hs2 ...");
    err = hand_shake2(par_sock, cliaddr, child_sock, childaddr.sin_port);
    if(err != EXIT_SUCCESS) {
        printf("There was an error with the handshake");
        free_clients(cliList);
        close(child_sock);
        close(par_sock);
        exit(EXIT_FAILURE);
    }

    /* init window */
    _DEBUG("%s\n", "init_wnd()");
    wnd  = init_wnd(start_seq);
    print_wnd((const char**)wnd);

    /* TODO: err = close(everything);*/
    close(par_sock);
    /* print out connection extablished */
    printf("connection established from child at -- ");
    print_sock_name(child_sock, &childaddr);
    printf("connection established to client at  -- ");
    print_sock_peer(child_sock, &cliaddr);

    _DEBUG("%s\n", "Sending file ...");
    err = send_file(fname, child_sock, wnd);
    if(err < 0) {
        _DEBUG("%s\n", "Something went wrong while sending the file.");
    }

    while(!is_wnd_empty()) {
        _DEBUG("%s\n", "QUITING: waiting for unACKed pkts");
        err = get_aks(wnd, child_sock, 1);
        if(err < 0) {
            _DEBUG("%s\n", "there was a problem getting ACKs");
        } else {
            _DEBUG("%s\n", "got ACK");
        }

        print_wnd((const char **)wnd);
    }

    _DEBUG("%s\n", "sending FIN");
    ++seq;
    /*srvsend(child_sock, FIN, NULL, 0, wnd);*/
    send_fin(child_sock);

    _DEBUG("%s\n", "cleaning up sockets ...");
    close(child_sock);
    _DEBUG("%s\n", "cleaning up client list ...");
    free_clients(cliList);
    _DEBUG("%s\n", "cleaning up window ...");
    free_wnd(wnd);
    _DEBUG("%s\n", "Exiting child");
    exit(EXIT_SUCCESS);
}

void send_fin(int sock) {
    ssize_t err;
    void* pkt = malloc(sizeof(struct xtcphdr));
    make_pkt(pkt, FIN, advwin, NULL, 0);
    printf("SENDING: ");
    print_hdr(pkt);
    htonpkt(pkt);
    do {
        err = send(sock, pkt, sizeof(struct xtcphdr), 0);
    } while(errno == EINTR && err < 0);
    if(err < 0) {
        perror("send_fin()");
    }

    free(pkt);
}

void proc_exit(int i) {
    int stat = i;
    pid_t pid;
    _DEBUG("%s\n", "child died");

    for(EVER) {
        pid = waitpid(-1, &stat, WNOHANG);
        if (pid == 0)
            return;
        else if (pid == -1)
            return;
        else if (WIFEXITED(stat)) {
            printf("Return code of child: %d\n", WEXITSTATUS(stat));
            stat = remove_client(&cliList, pid);
            if(stat < 0) {
                _DEBUG("%s\n", "there was an error removing the child");
            }

        } else if(WIFSIGNALED(stat)) {
            _DEBUG("dead child sig: %s\n", strsignal(WTERMSIG(stat)));

        } else {
            printf("proc_exit(): There was an error");
        }
    }
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

void free_clients(struct client_list* head) {
    if(head->next != NULL)
        free_clients(head->next);

    free(head);
}

int remove_client(struct client_list** head, pid_t pid) {
    struct client_list* at;
    struct client_list* prev = NULL;
    at = (*head);

    while(at != NULL) {
        if(at->pid == pid) {
            if((*head) == at) {
                (*head)= (*head)->next;
                free(at);
                _DEBUG("%s\n", "found client at the head");
                return 0;
            } else {
                prev->next = at->next;
                free(at);
                _DEBUG("%s\n", "found client NOT at the head");
                return 0;
            }
        }
        prev = at;
        at = at->next;
    }
    _DEBUG("%s\n", "did NOT find client");
    return -1;
}

int hand_shake2(int par_sock, struct sockaddr_in cliaddr, int child_sock, in_port_t newport) {
    ssize_t n;
    socklen_t len;
    char pktbuf[BUFF_SIZE];
    struct xtcphdr* hdr = malloc(sizeof(struct xtcphdr) + 2);
    uint16_t flags = 0;
    fd_set rset;
    int err;
    int count = 0;
    struct timeval timer;


    srand48(1);
    seq = (uint32_t)lrand48();

    flags = SYN|ACK;
    ++seq;
    ++ack_seq;

    _DEBUG("child| ACK: %" PRIu32 " SEQ: %" PRIu32 "\n", ack_seq, seq);

    make_pkt(hdr, flags, advwin, &newport, 2);
    printf("sent hs2: ");
    print_hdr(hdr);
    htonpkt(hdr);

    len = sizeof(struct sockaddr_in);
    do {
        n = sendto(par_sock, hdr, sizeof(struct xtcphdr) + 2, 0, (struct sockaddr const *)&cliaddr, len);
    } while(errno == EINTR && n < 0);
    if (n < 1) {
        perror("hs2.send(port)");
        free(hdr);
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "waiting to get reply on new child port ...");
    /*todo: use timer*/

redo_hs1:
    timer.tv_sec = 2;
    timer.tv_usec = 0;

    FD_ZERO(&rset);
    FD_SET(child_sock, &rset);
    err = select(child_sock + 1, &rset, NULL, NULL, &timer);
    if(err < 0) {
        if(errno == EINTR) {
            _DEBUG("%s\n", "select interupted, redo ...");
            goto redo_hs1;
        }
        perror("server.hs2()");
        free(hdr);
        exit(EXIT_FAILURE);
    }

    _DEBUG("select(): %d\n", err);

    while(!FD_ISSET(child_sock, &rset) && count < 4) {
        count++;
        _DEBUG("%s\n", "hs2 time out, re-sending over both sockets ...");

        len = sizeof(cliaddr);
        do {
            n = sendto(par_sock, hdr, sizeof(struct xtcphdr) + 2, 0, (struct sockaddr const *)&cliaddr, len);
        } while(errno == EINTR && n < 0);
        if (n < 1) {
            perror("hs2.send(port)");
            free(hdr);
            exit(EXIT_FAILURE);
        }
        /* child_sock connected to new_addr? */
        do {
            n = send(child_sock, hdr, sizeof(struct xtcphdr) + 2, 0);
        } while(errno == EINTR && n < 1);
        if (n < 1) {
            perror("hs2.send(port)");
            free(hdr);
            exit(EXIT_FAILURE);
        }
        _DEBUG("%s\n", "waiting to get reply on new child port ...");

redo_hs2:
        timer.tv_sec = 2;
        timer.tv_usec = 0;
        FD_ZERO(&rset);
        FD_SET(child_sock, &rset);
        err = select(child_sock + 1, &rset, NULL, NULL, &timer);
        if(err < 0) {
            if(errno == EINTR) {
                _DEBUG("%s\n", "select interupted, redo ...");
                goto redo_hs2;
            }
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

    do {
        n = recv(child_sock, pktbuf, sizeof(pktbuf), 0);
        if (n < 0 && errno != EINTR) {
            perror("hs2.recvfrom()");
            close(child_sock);
            exit(EXIT_FAILURE);
        }
    } while(errno == EINTR && n < 0);

    ntohpkt((struct xtcphdr*) pktbuf);
    printf("GOT: ");
    print_hdr((struct xtcphdr *) pktbuf);
    if(((struct xtcphdr*) pktbuf)->flags != ACK
            || ((struct xtcphdr*) pktbuf)->ack_seq != (seq + 1)
            || ((struct xtcphdr*) pktbuf)->seq != ack_seq) {
        printf("client's flags||ack||seq are wrong, handshake broken\n");
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "got last hand shake from client");
    start_seq = ((struct xtcphdr*) pktbuf)->ack_seq;
    _DEBUG("set start_seq to: %" PRIu32 "\n", start_seq);

    return EXIT_SUCCESS;
}


int send_file(char* fname, int sock, char **wnd) {
    FILE *file;
    char data[MAX_DATA_SIZE] = {0};  /* buffer for sending file and getting ACKs */
    size_t n;
    int err, tally, save_data;

    save_data = 0;
    tally = 0;
    n = 0;

    file = fopen(fname, "r");
    if(file == NULL) {
        fprintf(stderr, "send_file.fopen(%s",fname);
        perror(")");
        return EXIT_FAILURE;
    }

    for(EVER) {
        if(save_data) {
            _DEBUG("%s\n", "sending old data ...");
            goto saving_data;
        }
        n = fread(data, 1, MAX_DATA_SIZE, file);
        tally += n;
        _DEBUG("send_file.fread(%lu)\n", (unsigned long)n);
        if (ferror(file)) {
            printf("server.send_file(): There was an error reading the file\n");
            clearerr(file);
            fclose(file);
            return EXIT_FAILURE;
        } else if(feof(file) && n < 1) {
            printf("File finished uploading ...\n");
            break;
        } else {
            ++seq;
saving_data:
            _DEBUG("sending %lu bytes of file\n", (unsigned long) n);

            err = srvsend(sock, 0, data, n, wnd, !save_data, &cli_wnd);
            if(err == -1) {                                                           /* the error code for full window */
                save_data = 1;
                err = get_aks(wnd, sock, 0);
                if(err < 0) {
                    _DEBUG("%s\n", "there is something wrong woth the AKS");
                    return EXIT_FAILURE;
                }

            } else if(err < 0) {
                printf("server.send_file(): there was an error sending the file\n");
            } else {
                save_data = 0;
            }
        }
    }

    _DEBUG("tally: %d\n", tally);
    fclose(file);

    return EXIT_SUCCESS;
}

int get_aks(char** wnd, int sock, int always_block) {
    void* pkt;
    int err;
    int flag;
    int acks = 0;

    pkt = malloc(MAX_PKT_SIZE);
    if(pkt == NULL) {
        perror("get_aks(): ");
        return -1;
    }

    for(EVER) {                                                           /* listen for ACKs */
        print_wnd((const char**) wnd);
        if((is_wnd_full() || always_block) && !is_wnd_empty()) {                                               /* see if we need to wait for the window */
            flag = 0;
            _DEBUG("%s\n", "wnd IS full or quitting, recv WILL block");
        } else {
            flag = MSG_DONTWAIT;
            _DEBUG("%s\n", "wnd NOT full, recv WONT block: MSG_DONTWAIT");
        }

        do {
            err = (int) recv(sock, pkt, MAX_PKT_SIZE, flag);
        } while(errno == EINTR && err < 0);
        if (err < 0) {
            if(errno != EWOULDBLOCK) {                                     /* there was actually an error */
                fprintf(stderr, "send_file.get_ack(%d", err);
                perror(")");
                free(pkt);
                return -1;
            } else {                                                       /* no ACKs */
                _DEBUG("EWOULDBLOCK: ACKs recvd: %d\n", acks);
                break;
            }
        } else {                                                           /* got an ACK */
            ntohpkt(pkt);
            printf("GOT: ");
            print_hdr(pkt);
            _DEBUG("got an ACK: SEQ: %" PRIu32 " ACK: %" PRIu32 "\n",
                    ((struct xtcphdr *)pkt)->seq,
                    ((struct xtcphdr *)pkt)->ack_seq);

            err = handle_ack((struct xtcphdr *)pkt, wnd);
            if(err < 0) {
                _DEBUG("%s\n", "There was something wrong with client ACK");
                _DEBUG("ACKs recvd before error: %d\n", acks);
                return -1;
            }
        }
    }

    free(pkt);
    return acks;
}

int handle_ack(struct xtcphdr* pkt, char** wnd) {
    uint32_t pkt_ack = pkt->ack_seq;
    int count = 0;

    while(ge_base(pkt_ack -1)) {
        count++;
        _DEBUG("eating pkts until SEQ: %" PRIu32 "\n",  pkt_ack);
        cli_wnd = pkt->advwin;
        _DEBUG("Reset cli_wnd: %" PRIu16 "\n",  cli_wnd);

        remove_from_wnd((const char **) wnd);
    }

    /*if(count == 0) count = -1;*/
    _DEBUG("ate %d pkts\n", count);
    return count;
}
