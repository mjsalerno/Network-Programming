#include "common.h"
#include "ODR.h"
#include "debug.h"

void fill_mesg(struct Mesg *m, char* ip, int port, char* data, size_t data_len, int flag);

static struct sockaddr_un odr_addr = (struct sockaddr_un) {
        .sun_family = AF_LOCAL,
        .sun_path = ODR_PATH
};
static socklen_t odr_len = sizeof(odr_addr);

/*
msg_recv function which will do a (blocking) read on the application domain socket and return with :
int    giving socket descriptor for read
char*  giving message received
size_t len of the msg
char*  giving ‘canonical’ IP address for the source node of message, in presentation format
int*   giving source ‘port’ number
*/

ssize_t msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port) {
    return recv(sock, msg, msg_len, 0);
}

/*
msg_send function that will be called by clients/servers to send requests/replies. The parameters of the function consist of :
int         giving the socket descriptor for write
char*       giving the ‘canonical’ IP address for the destination node, in presentation format
int         giving the destination ‘port’ number
char*       giving message to be sent
size_t      len of the msg
int flag    if set, force a route rediscovery to the destination node even if a non-‘stale’ route already exists (see below)
 */
ssize_t msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag) {
    struct Mesg *m;
    ssize_t rtn;

    m = malloc(MESG_OFFSET + msg_len);
    if(m == NULL) {
        _ERROR("%s", "malloc()\n");
        exit(EXIT_FAILURE);
    }
    fill_mesg(m, ip, port, msg, msg_len, flag);

    rtn = sendto(sock, m, (MESG_OFFSET + msg_len), 0, (struct sockaddr*)&odr_addr, odr_len);

    free(m);
    return rtn;
}

void fill_mesg(struct Mesg *m, char* ip, int port, char* data, size_t data_len, int flag) {
    _DEBUG("%s", "Filling Mesg.\n");
    m->port = port;
    m->flag = flag;
    m->len = data_len;
    strncpy(m->ip, ip, INET_ADDRSTRLEN);
    memcpy(m + MESG_OFFSET, data, data_len);
}
