#include <stdlib.h>
#include <stdio.h>

#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include "server.h"
#include "rtt.h"
static struct rtt_info rttinfo;

int mmmain(void) {

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


void timer_handler (int signum) {
    static int count = 0;
    suseconds_t ustimestamp = rtt_ts(&rttinfo);
    printf ("timer expired %d times\n", ++count);
    printf ("last time stamp: %ld\n", ustimestamp);
}
/* rtt_main tests for rtt*/
int main(){
    struct sigaction sa = {0};
    struct itimerval timer;
    int err;


    suseconds_t op1 = 1, op2 = 6;
    suseconds_t res = op1 - op2;
    printf("suseconds_t calc: %ld\n", res);
    printf("sizeof(suseconds_t): %ld\n", sizeof(suseconds_t));




    /* Install timer_handler as the signal handler for SIGVTALRM. */
    sa.sa_handler = &timer_handler;
    sigaction (SIGALRM, &sa, NULL);

    /* Configure the timer to expire after 250 msec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 750000;
    /* ... and every never after that. */
    timer.it_interval = (struct timeval){0, 750000};

    rtt_init(&rttinfo);
    setitimer(ITIMER_REAL, &timer, NULL);

    /* Do busy work. */
    while (1){
        err = sleep(5);
        printf("secs left to sleep %d\n", err);
    }

    return 0;
}
