#include "server.h"

int main(int argc, const char **argv) {
    const char *path;

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
    port = int_from_config(file, "There was an error getting the port number");
    if(port < 1) {
        printf("The port can not be less than 1\n");
        printf("The port can not be less than 1\n");
        return EXIT_FAILURE;
    } 
    printf("Port: %hu\n", port);
    
    /*Get the window size*/
    window = int_from_config(file, "There was an error getting the window size number");
    if(window < 1) {
        printf("The window can not be less than 1\n");
        return EXIT_FAILURE;
    }
    printf("Window: %ld\n", window);

    printf("config: %s\n", path);

    fclose(file);



    int sockfd,n;
    struct sockaddr_in servaddr,cliaddr;
    socklen_t len;
    char mesg[BUFF_SIZE];

    sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port=htons(port);
    bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

    for (;;) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
        sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
        mesg[n] = 0;
        printf("-------------------------------------------------------\n");
        printf("Received the following: '%s'\n", mesg);
        printf("-------------------------------------------------------\n");
    }



    return EXIT_SUCCESS;
}
