#ifndef	RTT_H
#define	RTT_H

/*#include	"unp.h" */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

struct rtt_info { /* fixme: add field for pre-doubled RTO */
    suseconds_t    rtt_rtt;	    /* most recent measured RTT, in us */
    suseconds_t    rtt_srtt;	/* smoothed RTT estimator, in us */
                                /* rtt_srtt stored scaled by 1/g i.e. 8 */

    suseconds_t    rtt_sdev;	/* smoothed mean deviation, in us */
                                /* rtt_sdev stored scaled by 1/h i.e. 4 */

    suseconds_t    rtt_rto;	    /* current RTO to use when rtt_nrexmt is 0, in us */
    suseconds_t    rtt_dub_rto;	/* the doubling RTO for timeouts, in us */
    int            rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
    time_t         rtt_base_sec;   /* sec since 1/1/1970 at start*/
    suseconds_t    rtt_base_usec;  /* us since 1/1/1970 at start*/
};

#define	RTT_RXTMIN      1000000	/* min retransmit timeout value, in us */
#define	RTT_RXTMAX      3000000	/* max retransmit timeout value, in us */
#define	RTT_MAXNREXMT 	12  	/* max # times to retransmit */

/* function prototypes */
void	    rtt_debug(struct rtt_info *);
void	    rtt_init(struct rtt_info *);
void	    rtt_newpack(struct rtt_info *);
suseconds_t rtt_ts(struct rtt_info *ptr);
void        rtt_start_timer(struct rtt_info *ptr, struct itimerval *itv);
void        rtt_stop(struct rtt_info *ptr, suseconds_t ts);
int		    rtt_timeout(struct rtt_info *);

#endif
