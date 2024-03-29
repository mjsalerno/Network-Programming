/* Our own header for the programs that need hardware address info. */
#ifndef GET_HW_ADDRS_H
#define GET_HW_ADDRS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>		        /* error numbers */
#include <sys/ioctl.h>          /* ioctls */
#include <net/if.h>             /* generic interface structures */

/* IFNAMSIZ    in <net/if.h> */
/* IFHWADDRLEN in <net/if.h> */

#define	IP_ALIAS  	 1	        /* hwa_addr is an alias */

struct hwa_info {
  char    if_name[IFNAMSIZ];	/* interface name, null terminated */
  char    if_haddr[IFHWADDRLEN];/* hardware address */
  int     if_index;		        /* interface index */
  short   ip_alias;		        /* 1 if hwa_addr is an alias IP address */
  struct  sockaddr  *ip_addr;	/* IP address */
  struct  hwa_info  *hwa_next;	/* next of these structures */
};


/* function prototypes */
struct hwa_info	*get_hw_addrs(void);
void free_hwa_info(struct hwa_info *hwahead);
struct hwa_info* find_hwa(int index, struct hwa_info* head);

#endif /* GET_HW_ADDRS_H */
