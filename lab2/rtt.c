/* include rtt1 */
#include "rtt.h"
#include "debug.h"

int		rtt_d_flag = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) ((ptr->rtt_srtt >> 3) + ptr->rtt_sdev)

/* note: both gettimeofday and setitimer operate on time_t (s) and suseconds_t (us) */

static suseconds_t rtt_minmax(suseconds_t rto) {
    if (rto < RTT_RXTMIN) {
        rto = RTT_RXTMIN;
    }
    else if (rto > RTT_RXTMAX) {
        rto = RTT_RXTMAX;
    }
    return(rto);
}

void rtt_init(struct rtt_info *ptr) {
    struct timeval	tv;
    int err;

    err = gettimeofday(&tv, NULL);
    if(err < 0){
        perror("rtt_init.gettimeofday()");
        exit(EXIT_FAILURE);
    }
    ptr->rtt_base_sec = tv.tv_sec;      /* # sec since 1/1/1970, updated on rtt_start_timer() */
    ptr->rtt_base_usec = tv.tv_usec;    /* # us since 1/1/1970, updated on rtt_start_timer()  */

    ptr->rtt_nrexmt = 0;
    ptr->rtt_rtt    = 0;
    ptr->rtt_srtt   = 0;
    ptr->rtt_sdev   = 250000 << 2; /* to start the rtt_rto at 1 second */
    ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
    /* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}

/*
 * Return the current timestamp.
 * Our timestamps are suseconds_t's that count microseconds since
 * rtt_init() was called.
 */
suseconds_t rtt_ts(struct rtt_info *ptr) {
    suseconds_t		ts;
    struct timeval	tv;
    int err;

    err = gettimeofday(&tv, NULL);
    if(err < 0){
        perror("rtt_ts.gettimeofday()");
        exit(EXIT_FAILURE);
    }

    ts = ((tv.tv_sec - ptr->rtt_base_sec) * 1000000) + ((tv.tv_usec - ptr->rtt_base_usec));
    return(ts);
}

void rtt_newpack(struct rtt_info *ptr) {
    ptr->rtt_nrexmt = 0;
}


/**
* Fills in itv with the current RTO.

* USE AS:
* struct itimerval newtimer;
* rtt_start_timer(ptr, &newtimer);
* setitimer(ITIMER_REAL, &newtimer, NULL)
*
*/
void rtt_start_timer(struct rtt_info *ptr, struct itimerval *itv) {
    struct timeval	tv;
    int err;
    suseconds_t  startrto = rtt_minmax((ptr->rtt_rto + 500000));
    _NOTE("start rto is %ld\n", (long)startrto);
    /*itv->it_value.tv_sec = startrto >> 20;*/ /* 2^20 usecs is almost 1 sec */
    if(startrto >= 3000000) {
        itv->it_value.tv_sec = 3;
        itv->it_value.tv_usec = 0;
    } else if(startrto >= 2000000) {
        itv->it_value.tv_sec = 2;
        itv->it_value.tv_usec = startrto - 2000000;
    } else if(startrto >= 1000000){
        itv->it_value.tv_sec = 1;
        itv->it_value.tv_usec = startrto - 1000000;
    } else {
        itv->it_value.tv_sec = 1;
        itv->it_value.tv_usec = 0;
    }
    itv->it_interval.tv_sec = 0;
    itv->it_interval.tv_usec = 0;
    _NOTE("timer filled for %ld.%06ldsecs\n", (long)itv->it_value.tv_sec, (long)itv->it_value.tv_usec);
    rtt_debug(ptr);

    /* move our base time forward */
    err = gettimeofday(&tv, NULL);
    if(err < 0){
        perror("rtt_ts.gettimeofday()");
        exit(EXIT_FAILURE);
    }
    ptr->rtt_base_sec = tv.tv_sec;
    ptr->rtt_base_usec = tv.tv_usec;
}

/*
 * Subsuquent calls to rtt_stop() must be followed by a call to rtt_start_timer()
 *
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 *
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 *
 * NOTE suseconds_t is a signed type
 */
void rtt_stop(struct rtt_info *ptr) {
    suseconds_t m = rtt_ts(ptr);
    ptr->rtt_rtt = m; /* update last measured RTT in usecs */

    /*
     * Update our estimators of RTT and mean deviation of RTT.
     * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
     ************************
     * For the below algorithm:
     * sa short for "scaled average"
     * sv short for "scaled mean deviation"
     * For us:
     * m --- Err (the current measured rtt)
     * sa --- rtt_srtt
     * sv --- rtt_sdev
     * rto --- rtt_rto
     ************************
     *  m −= (sa >> 3);
     *  sa += m;
     *  if (m < 0){
     *      m = −m;
     *  }
     *  m −= (sv >> 2);
     *  sv += m;
     *  rto = (sa >> 3) + sv;
     ************************
     */

    m -= (ptr->rtt_srtt >> 3); /* rtt_srtt stored scaled by 1/g i.e. 8 */
    ptr->rtt_srtt += m;
    if (m < 0) {
        m = -m;
    }
    m -= (ptr->rtt_sdev >> 2);
    ptr->rtt_sdev += m;
    ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
    rtt_debug(ptr);
}

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 *
 * In function rtt_timeout (Fig. 22.14), after doubling the RTO in line 86,
 * pass its value through the function rtt_minmax of Fig. 22.11 (somewhat along
 * the lines of what is done in line 77 of rtt_stop, Fig. 22.13).
 */
int rtt_timeout(struct rtt_info *ptr) {
    ptr->rtt_rto <<= 1;		/* double next RTO */
    ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);

    if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
        return(-1);			/* time to give up for this packet */
    rtt_debug(ptr);
    return(0);
}

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */
void rtt_debug(struct rtt_info *ptr) {

    _NOTE("rtt = %ldus, srtt>>3 = %ldus, sdev>>2 = %ldus, rto = %ldus\n",
            ptr->rtt_rtt, ptr->rtt_srtt >> 3, ptr->rtt_sdev >> 2, ptr->rtt_rto);
}
