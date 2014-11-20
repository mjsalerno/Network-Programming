#include "common.h"
#include "ODR.h"
#include "debug.h"

void fill_mesg(struct odr_msg *m, char* ip, int port, char* msg, size_t msg_len, int flag);

static struct sockaddr_un odr_addr = (struct sockaddr_un) {
        .sun_family = AF_LOCAL,
        .sun_path = ODR_PATH
};
static socklen_t odr_len = sizeof(odr_addr);

/*
msg_recv    function which will do a (blocking) read on the application domain socket and return with :
int         giving socket descriptor for read
char*       giving message received
size_t      len of the msg
char*       giving ‘canonical’ IP address for the source node of message, in presentation format
int*        giving source ‘port’ number
*/

ssize_t msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port) {
    /* todo: use recvfrom(), check if src was ODR_PATH */

    ssize_t n;
    char buf_msg[ODR_MSG_MAX];        /* space to recv from ODR */
    struct odr_msg m;                      /* space to place stuff from buf_msg */
    n = recv(sock, buf_msg, ODR_MSG_MAX, 0);
    if(n < 0) {
        return n;
    } else if(n < (ssize_t)sizeof(struct odr_msg)) {
        _ERROR("recv'd odr_msg was too short!! n: %d\n", (int)n);
        exit(EXIT_FAILURE);
    }
    memcpy(&m, buf_msg, sizeof(m));     /* copy into struct, no SIGBUS */

    if(n != (ssize_t)(m.len + sizeof(struct odr_msg))) {
        _ERROR("recv'd bytes doesn't add up! n: %ld, m.len %d\n", (long int)n, m.len);
    }
    strncpy(ip, m.src_ip, INET_ADDRSTRLEN);                 /* fill user's ip */
    *port = m.src_port;                                     /* fill user's port */
    /* fill user's msg buff */
    memcpy(msg, (buf_msg + sizeof(struct odr_msg)), ((m.len > msg_len) ? msg_len : (size_t)m.len));
    if(msg_len < m.len) {
        _ERROR("mesg was truncated, user's msg_len: %ld  <  m.len: %ld", (long int)msg_len, (long int)m.len);
    }
    _DEBUG("GOT: from %s:%d\n", m.src_ip, m.src_port);
    return m.len;
}

/*
msg_send    function that will be called by clients/servers to send requests/replies. The parameters of the function consist of :
int         giving the socket descriptor for write
char*       giving the ‘canonical’ IP address for the destination node, in presentation format
int         giving the destination ‘port’ number
char*       giving message to be sent
size_t      len of the msg
int flag    if set, force a route rediscovery to the destination node even if a non-‘stale’ route already exists (see below)
 */
ssize_t msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag) {
    struct odr_msg *m;
    ssize_t rtn;

    m = malloc(sizeof(struct odr_msg) + msg_len);
    if(m == NULL) {
        _ERROR("%s", "malloc()\n");
        exit(EXIT_FAILURE);
    }
    memset(m, 0, sizeof(struct odr_msg) + msg_len);
    fill_mesg(m, ip, port, msg, msg_len, flag);
    m->type = 2;
    _DEBUG("SEND: to %s:%d, force_redisc = %d\n", m->dst_ip, m->dst_port, m->force_redisc);
    rtn = sendto(sock, m, (sizeof(struct odr_msg) + msg_len), 0, (struct sockaddr*)&odr_addr, odr_len);
    free(m);
    return rtn;
}

/* used by only msg_send() for now */
void fill_mesg(struct odr_msg *m, char* ip, int port, char* msg, size_t msg_len, int flag) {
    /*_DEBUG("%s", "Filling odr_msg.\n");*/
    /* note: dst ip and port for msg_send() */
    m->dst_port = (uint16_t)port;
    m->force_redisc = (uint8_t)flag;
    m->len = (uint16_t)msg_len;
    strncpy(m->dst_ip, ip, INET_ADDRSTRLEN);
    /* copy to the end of m so (m+1) */
    memcpy(m + 1, msg, msg_len);
}
