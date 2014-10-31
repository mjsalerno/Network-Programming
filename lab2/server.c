#include <time.h>
#include <setjmp.h>
#include "server.h"
#include "rtt.h"

struct client_list* cliList;

static uint32_t start_seq;
static struct iface_info* ifaces;

extern struct rtt_info   rttinfo;
extern struct itimerval newtimer;
static sigjmp_buf	jmpbuf;
static struct window* wnd;
static uint16_t ini_advwin;
static uint32_t ini_seq;

uint32_t ini_ack_seq;

int main(int argc, const char **argv) {
    const char *path;

    FILE *file;
    unsigned short port;
    fd_set fdset;
    int pid;
    int err;
    int max_fd;
    int cur_sock;
    ssize_t n;
    struct client_list* newCli;
    struct iface_info* ptr;
    sigset_t sigset;

    struct sockaddr_in cliaddr;/*, servaddr, p_serveraddr, p_cliaddr;*/
    socklen_t len;
    char pkt[MAX_PKT_SIZE + 1];

    cliList = NULL;
    ifaces = NULL;

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
    ini_advwin = (uint16_t)int_from_config(file, "There was an error getting the window size number");
    if(ini_advwin < 1) {
        fprintf(stderr, "The window can not be less than 1\n");
        exit(EXIT_FAILURE);
    }
    _DEBUG("Window: %hu\n", ini_advwin);
    _DEBUG("config: %s\n", path);

    err = fclose(file);
    if(err < 0) {
        perror("server.fclose()");
    }

    /* do iffy info */
    ifaces = make_iface_list();
    bind_to_iface_list(ifaces, port);
    print_iface_list(ifaces);

    /* print_sock_name(sockfd, &p_serveraddr); */
    print_iface_list_sock_name(ifaces);

    if(SIG_ERR == signal(SIGCHLD, proc_exit)) {
        perror("server.signal()");
    }


    for(EVER) {

        max_fd = fd_set_iface_list(ifaces, &fdset);
        err = select(max_fd + 1, &fdset, NULL, NULL, NULL);
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

            ptr = fd_is_set_iface_list(ifaces, &fdset);
            if(ptr == NULL) {
                fprintf(stderr, "There was a problem getting the set socket in select\n");
                exit(EXIT_FAILURE);
            }

            cur_sock = ptr->sock;

            n = recvfrom(cur_sock, pkt, MAX_PKT_SIZE, 0, (struct sockaddr *) &cliaddr, &len);
            _DEBUG("out of recvfrom n: %d errno: %d\n", (int)n, errno);
            if (n < 0 && errno != EINTR) {
                perror("server.recvfrom()");
                fprintf(stderr, "continuing ...\n");
                continue;
            }
        } while(errno == EINTR && n < 0);

        ntohpkt((struct xtcphdr*)pkt);
        ini_ack_seq = ((struct xtcphdr*)pkt)->seq;

        pkt[n] = 0;
        if(((struct xtcphdr*)pkt)->flags != SYN) {
            printf("server.hs1(): client did not send SYN: %hu\n", ((struct xtcphdr*)pkt)->flags);
            continue;
        }

        /* block SIGCHLD */
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGCHLD);
        sigprocmask(SIG_BLOCK, &sigset, NULL);

        newCli = add_client(&cliList, cliaddr.sin_addr.s_addr, cliaddr.sin_port);
        if(newCli == NULL) {
            _DEBUG("%s\n", "Server: dup clinet, not doing anything");

            ntohpkt((struct xtcphdr*)pkt);
            printf("GOT: ");
            print_hdr((struct xtcphdr *) pkt);
            continue;
        }


        /* pick the smallest advwin */
        ini_advwin = ((struct xtcphdr*)pkt)->advwin < ini_advwin ? ((struct xtcphdr*)pkt)->advwin : ini_advwin;
        _DEBUG("new advwin: %d\n", ini_advwin);

        print_hdr((struct xtcphdr *) pkt);
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
            child(pkt + DATA_OFFSET, cur_sock, cliaddr);
            /* we should never get here */
            fprintf(stderr, "A child is trying to use the connection select\n");
            assert(0);
        }

        newCli->pid = pid;
        /* unblock SIGCHLD */
        sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    }


    return EXIT_SUCCESS;
}

int child(char* fname, int par_sock, struct sockaddr_in cliaddr) {
    struct sockaddr_in childaddr;
    struct iface_info* iface_ptr;
    int err;
    int child_sock;
    socklen_t len;
    struct sigaction sa;

    _DEBUG("%s\n", "In child");

    iface_ptr = get_iface_from_sock(ifaces, par_sock);
    if(iface_ptr == NULL) {
        _DEBUG("%s\n", "could not find the iface with that socket");
        return -1;
    }

    /* dont clost the par_sock */
    iface_ptr->sock = -1;

    _DEBUG("child.filename: %s\n", fname);
    child_sock = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&childaddr, sizeof(childaddr));
    childaddr.sin_family = AF_INET;
    childaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    childaddr.sin_port = htons(0);

    iface_ptr = get_matching_iface(ifaces, cliaddr.sin_addr.s_addr);
    if(iface_ptr == NULL) {
        _DEBUG("%s\n", "could not find the correct iface");
        childaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    } else {
        int optval = 1;
        _DEBUG("%s\n", "found local iface, using SO_DONTROUTE");
        err = setsockopt(child_sock, SOL_SOCKET, SO_DONTROUTE, &optval, sizeof(int));

        if(err < 0) {
            perror("child.setsockopt()");
            return -1;
        }
        childaddr.sin_addr.s_addr = iface_ptr->ip;
        _DEBUG("will bind to: %s\n", inet_ntoa(childaddr.sin_addr));
    }

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

    /* init window */
    _DEBUG("%s\n", "init_wnd()");
    wnd = init_window(3, start_seq, 1, 0, 0);
    print_window(wnd);

    _DEBUG("%s\n", "doing hs2 ...");
    err = hand_shake2(par_sock, cliaddr, child_sock, childaddr.sin_port);
    if(err != EXIT_SUCCESS) {
        printf("There was an error with the handshake");
        free_clients(cliList);
        close(child_sock);
        close(par_sock);
        exit(EXIT_FAILURE);
    }

    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &sig_alrm;
    sigaction (SIGALRM, &sa, NULL);

    /* TODO: err = close(everything);*/
    close(par_sock);
    /* print out connection extablished */
    printf("connection established from child at -- ");
    print_sock_name(child_sock, &childaddr);
    printf("connection established to client at  -- ");
    print_sock_peer(child_sock, &cliaddr);

    _DEBUG("%s\n", "Sending file ...");
    err = send_file(fname, child_sock);
    if(err < 0) {
        _DEBUG("%s\n", "Something went wrong while sending the file.");
    }

    while(!is_wnd_empty(wnd)) {
        _DEBUG("%s\n", "QUITING: waiting for unACKed pkts");
        err = recv_acks(child_sock, 1);
        if(err < 0) {
            _DEBUG("%s\n", "there was a problem getting ACKs");
        } else {
            _DEBUG("%s\n", "got ACK");
        }

        print_window(wnd);
    }

    _NOTE("%s\n", "sending FIN");

    if(is_wnd_full(wnd)) {
        /* need to wait untill we have enough window space */
        recv_acks(child_sock, 0);
    } else if(is_wnd_empty(wnd)) {
        /* the timer wont be called for us since we are the last pkt */
        rtt_newpack(&rttinfo);
        rtt_start_timer(&rttinfo, &newtimer);
    }
    srv_add_send(child_sock, NULL, 0, FIN, wnd);

    _NOTE("%s\n", "Waiting for client to ACK all pkts ...");
    recv_acks(child_sock, 1);

    _DEBUG("%s\n", "cleaning up sockets ...");
    close(child_sock);
    _DEBUG("%s\n", "cleaning up client list ...");
    free_clients(cliList);
    _DEBUG("%s\n", "cleaning up window ...");
    free_window(wnd);
    _DEBUG("%s\n", "Exiting child");
    exit(EXIT_SUCCESS);
}

void quick_send(int sock, uint16_t flags) {
    srv_add_send(sock, NULL, 0, flags, wnd);
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
            exit(EXIT_FAILURE);
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
        if(p == NULL) {
            perror("add_client.malloc()");
            exit(EXIT_FAILURE);
        }
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
    struct xtcphdr* hdr;
    uint16_t flags = 0;
    fd_set rset;
    int err;
    int count = 0;
    struct timeval timer;


    srand48(1);
    ini_seq = (uint32_t)lrand48();

    flags = SYN|ACK;

    _DEBUG("child| ACK: %" PRIu32 " SEQ: %" PRIu32 "\n", ini_ack_seq, ini_seq);

    hdr = alloc_pkt(++ini_seq, ++ini_ack_seq, flags, ini_advwin, &newport, 2);
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

    while(!FD_ISSET(child_sock, &rset) && count < 12) {
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
        timer.tv_sec = 0;
        timer.tv_usec = 50000 * MIN(count, 6);
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
    ntohpkt(hdr);
    wnd->servlastseqsent = hdr->seq;
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
    wnd->servlastackrecv =((struct xtcphdr*) pktbuf)->ack_seq;
    printf("GOT: ");
    print_hdr((struct xtcphdr *) pktbuf);
    if(((struct xtcphdr*) pktbuf)->flags != ACK
            || ((struct xtcphdr*) pktbuf)->ack_seq != (ini_seq + 1)
            || ((struct xtcphdr*) pktbuf)->seq != ini_ack_seq) {
        printf("client's flags||ack||seq are wrong, handshake broken\n");
        exit(EXIT_FAILURE);
    }

    _DEBUG("%s\n", "got last hand shake from client");
    start_seq = ((struct xtcphdr*) pktbuf)->ack_seq;
    _DEBUG("set start_seq to: %" PRIu32 "\n", start_seq);

    return EXIT_SUCCESS;
}

int send_file(char* fname, int sock) {
    FILE* file;
    size_t n;
    int tally = 0;
    char data[MAX_DATA_SIZE];

    file = fopen(fname, "r");
    if(file == NULL) {
        perror("new_send_file()");
        exit(EXIT_FAILURE);
    }

    for(EVER) {

        if(!is_wnd_full(wnd)) {
            n = fread(data, 1, MAX_DATA_SIZE, file);
            tally += n;
            _DEBUG("send_file.fread(%lu)\n", (unsigned long) n);
            if (ferror(file)) {
                printf("server.send_file(): There was an error reading the file\n");
                clearerr(file);
                fclose(file);
                return EXIT_FAILURE;

            } else if (feof(file) && n < 1) {
                printf("File finished uploading ...\n");
                break;
            }

            /* start timer for new pkt */
            if (is_wnd_empty(wnd)) {
                rtt_newpack(&rttinfo);
                rtt_start_timer(&rttinfo, &newtimer);
                _NOTE("%s\n", "the window is empty, refreshing timer");
            }

            srv_add_send(sock, data, n, 0,wnd);

            if (sigsetjmp(jmpbuf, 1) != 0) {
                if (rtt_timeout(&rttinfo) < 0) {
                    _ERROR("%s\n", "The packet has timed out, giving up\n");
                    quick_send(sock, RST);
                    exit(EXIT_FAILURE);
                } else {
                    _NOTE("%s\n", "Packet timeout, resending");
                    rtt_start_timer(&rttinfo, &newtimer);
                    wnd->ssthresh = MAX(wnd->cwin / 2, 1);
                    wnd->cwin = 1;
                    srv_send_base(sock, wnd);
                    break;
                }
            }
        } else {
            recv_acks(sock, 0);
        }
    }

    return 1;
}

int recv_acks(int sock, int always_block) {
    int flag;
    int err;
    int acks = 0;
    char pkt[MAX_PKT_SIZE];

    for(EVER) {

        if ((is_wnd_full(wnd) || always_block) && !is_wnd_empty(wnd)) {
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
                exit(EXIT_FAILURE);
            } else {                                                       /* no ACKs */
                _DEBUG("EWOULDBLOCK: ACKs recvd: %d\n", acks);
                break;
            }
        } else {                                                           /* got an ACK */
            switch(((struct xtcphdr*)pkt)->flags) {
                case FIN:
                    _NOTE("%s\n", "client sent me a FIN, quiting");
                    exit(EXIT_FAILURE);
                case RST:
                    _NOTE("%s\n", "client sent me a RST, quiting");
                    exit(EXIT_FAILURE);
                case ACK:
                    break;
                default:
                    _ERROR("client sent me bad flag: %" PRIu16 ", quiting", ((struct xtcphdr*)pkt)->flags);
                    break;
            }
            acks++;
            ntohpkt((struct xtcphdr*)pkt);
            printf("GOT: ");
            print_hdr((struct xtcphdr*)pkt);
            _DEBUG("got an ACK: SEQ: %" PRIu32 " ACK: %" PRIu32 "\n",
                    ((struct xtcphdr *)pkt)->seq,
                    ((struct xtcphdr *)pkt)->ack_seq);

            new_ack_recvd(wnd, (struct xtcphdr*)pkt);
        }
    }

    return acks;
}

static void sig_alrm(int signo) {
    signo++;
    siglongjmp(jmpbuf, 1);
}
