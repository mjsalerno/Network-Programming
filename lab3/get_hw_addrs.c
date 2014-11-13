#include "get_hw_addrs.h"

/* internal helper */
struct hwa_info *Get_hw_addrs() {
    struct hwa_info	*hwa;

    if ( (hwa = get_hw_addrs()) == NULL) {
        fprintf(stderr, "ERROR: get_hw_addrs()\n");
        exit(EXIT_FAILURE);
    }
    return hwa;
}

/* Uses ioctl(2) to get MAC and H/W info about all interfaces */
struct hwa_info *get_hw_addrs(void) {
	struct hwa_info	*hwa, *hwahead, **hwapnext;
	int	sockfd, len, lastlen, nInterfaces, i;
    short alias; /* is an alias? */
	char *buf, lastname[IFNAMSIZ], *cptr;
	struct ifconf ifc;
	struct ifreq *ifr, *item, ifrcopy;
	struct sockaddr	*sinptr;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        perror("ERROR: socket()");
        exit(EXIT_FAILURE);
    }

	lastlen = 0;
	len = 100 * sizeof(struct ifreq);	/* initial buffer size guess */
	for ( ; ; ) {
		buf = malloc((size_t)len);
        if(buf == NULL) {
            fprintf(stderr, "ERROR malloc returned NULL");
            exit(EXIT_FAILURE);
        }
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
			if (errno != EINVAL || lastlen != 0) {
                perror("ERROR: ioctl()");
                exit(EXIT_FAILURE);
            }
		} else {
			if (ifc.ifc_len == lastlen)
				break;		/* success, len has not changed */
			lastlen = ifc.ifc_len;
		}
		len += 10 * sizeof(struct ifreq);	/* increment */
		free(buf);
	}

	hwahead = NULL;
	hwapnext = &hwahead;
	lastname[0] = 0;
    
	ifr = ifc.ifc_req;
	nInterfaces = ifc.ifc_len / (int)sizeof(struct ifreq);
	for(i = 0; i < nInterfaces; i++)  {
		item = &ifr[i];
 		alias = 0; 
		hwa = (struct hwa_info *) calloc(1, sizeof(struct hwa_info));
        if(hwa == NULL) {
            fprintf(stderr, "ERROR calloc returned NULL");
            exit(EXIT_FAILURE);
        }
		memcpy(hwa->if_name, item->ifr_name, IFNAMSIZ);		/* interface name */
		hwa->if_name[IFNAMSIZ-1] = '\0';
				/* start to check if alias address */
		if ( (cptr = (char *) strchr(item->ifr_name, ':')) != NULL)
			*cptr = 0;		/* replace colon will null */
		if (strncmp(lastname, item->ifr_name, IFNAMSIZ) == 0) {
			alias = IP_ALIAS;
		}
		memcpy(lastname, item->ifr_name, IFNAMSIZ);
		ifrcopy = *item;
		*hwapnext = hwa;		/* prev points to this new one */
		hwapnext = &hwa->hwa_next;	/* pointer to next one goes here */

		hwa->ip_alias = alias;		/* alias IP address flag: 0 if no; 1 if yes */
                sinptr = &item->ifr_addr;
		hwa->ip_addr = (struct sockaddr *) calloc(1, sizeof(struct sockaddr));
        if(hwa->ip_addr == NULL) {
            fprintf(stderr, "ERROR calloc returned NULL");
            exit(EXIT_FAILURE);
        }
	        memcpy(hwa->ip_addr, sinptr, sizeof(struct sockaddr));	/* IP address */
		if (ioctl(sockfd, SIOCGIFHWADDR, &ifrcopy) < 0)
                          perror("SIOCGIFHWADDR");	/* get hw address */
		memcpy(hwa->if_haddr, ifrcopy.ifr_hwaddr.sa_data, IFHWADDRLEN);
		if (ioctl(sockfd, SIOCGIFINDEX, &ifrcopy) < 0)
                          perror("SIOCGIFINDEX");	/* get interface index */
		memcpy(&hwa->if_index, &ifrcopy.ifr_ifindex, sizeof(int));
	}
	free(buf);
    close(sockfd);
	return hwahead;	/* pointer to first structure in linked list */
}

void free_hwa_info(struct hwa_info *hwahead) {
	struct hwa_info	*hwa, *hwa_next;

	for (hwa = hwahead; hwa != NULL; hwa = hwa_next) {
		free(hwa->ip_addr);
		hwa_next = hwa->hwa_next;	/* can't fetch hwa_next after free() */
		free(hwa);			        /* the hwa_info{} itself */
	}
}
