/*
msg_recv function which will do a (blocking) read on the application domain socket and return with :
int    giving socket descriptor for read
char*  giving message received
size_t len of the msg
char*  giving ‘canonical’ IP address for the source node of message, in presentation format
int*   giving source ‘port’ number
*/

ssize_t void msg_recv(int sock, char* msg, size_t msg_len, char* ip, int* port) {
    return -1;
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
ssize_t void msg_send(int sock, char* ip, int port, char* msg, size_t msg_len, int flag) {
    return -1;
}