#include "get_hw_addrs.h"
#include "debug.h"

/* internal helper */
struct hwa_info *Get_hw_addrs() {
    struct hwa_info	*hwa;

    if ( (hwa = get_hw_addrs()) == NULL) {
        fprintf(stderr, "ERROR: get_hw_addrs()\n");
        exit(EXIT_FAILURE);
    }
    return hwa;
}

struct hwa_info* find_hwa(int index, struct hwa_info* head) {
    struct hwa_info* ptr;
    for(ptr = head; ptr != NULL; ptr = ptr->hwa_next) {
        if(ptr->if_index == index) {
            return ptr;
        }
    }

    return NULL;
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

void print_hw_addrs(struct hwa_info	*hwahead) {
	struct hwa_info	*hwa_curr;
	struct sockaddr	*sa;
	char   *ptr, straddrbuf[INET6_ADDRSTRLEN];
	int    i, prflag;

	hwa_curr = hwahead;

	printf("\n");
	for(; hwa_curr != NULL; hwa_curr = hwa_curr->hwa_next) {

		printf("%s :%s", hwa_curr->if_name, ((hwa_curr->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");

		if ((sa = hwa_curr->ip_addr) != NULL) {
			switch(sa->sa_family) {
				case AF_INET:
					inet_ntop(AF_INET, &((struct sockaddr_in *) sa)->sin_addr, straddrbuf, INET_ADDRSTRLEN);
					printf("         IP addr = %s\n", straddrbuf);
					break;
				case AF_INET6:
					inet_ntop(AF_INET6, &((struct sockaddr_in6 *) sa)->sin6_addr, straddrbuf, INET6_ADDRSTRLEN);
					printf("         IP6 addr = %s\n", straddrbuf);
					break;
				default:
					break;
			}
		}

		prflag = 0;
		i = 0;
		do {
			if(hwa_curr->if_haddr[i] != '\0') {
				prflag = 1;
				break;
			}
		} while(++i < IFHWADDRLEN);

		if(prflag) {                        /* print the MAC address ex) FF:FF:FF:FF:FF:FF */
			printf("         HW addr = ");
			ptr = hwa_curr->if_haddr;
			i = IFHWADDRLEN;
			do {
				printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
			} while(--i > 0);
			printf("\n");
		}

		printf("         interface index = %d\n\n", hwa_curr->if_index);
	}
}

void add_mips(struct hwa_ip ** mip_head, char if_haddr[IFHWADDRLEN], struct sockaddr_in  *ip_addr, int index) {
	struct hwa_ip* curr = *mip_head;
	struct hwa_ip* prev = curr;

	for(; curr != NULL; prev = curr, curr = curr->next);

	if(prev == NULL) {
		curr = malloc(sizeof(struct hwa_ip));
		*mip_head = curr;
	} else {
		prev->next = malloc(sizeof(struct hwa_ip));
		curr = prev->next;
	}

	memset(curr, 0, sizeof(struct hwa_ip));
	curr->if_index = index;
	curr->next = NULL;
	memcpy(curr->if_haddr, if_haddr, IFHWADDRLEN);
	memcpy(&curr->ip_addr, ip_addr, sizeof(struct sockaddr_in));

}


/**
* Removes all but eth0 from the list of interfaces.
* Stores the IP of eth0 into the static host_ip
*/
void keep_eth0(struct hwa_info	**hwahead, struct hwa_ip ** mip_head) {
	struct hwa_info *curr;
	curr = *hwahead;

	while (curr != NULL) {
		_DEBUG("Looking for '%s'\n", IFACE_TO_KEEP);
		if (0 == strcmp(curr->if_name, IFACE_TO_KEEP)) {
			_DEBUG("Leaving interface %s in the interface list.\n", curr->if_name);
			add_mips(mip_head, curr->if_haddr, (struct sockaddr_in*)curr->ip_addr, curr->if_index);
		} else {
			_DEBUG("Skipping interface %s in the interface list.\n", curr->if_name);
		}
		curr = curr->hwa_next;
	}
}

void print_hwa_list(struct hwa_ip* head) {

	for(; head != NULL; head = head->next) {
		print_hwa_ip(head);
	}

}

void print_hwa_ip(struct hwa_ip* node) {
	printf("IP : %s\nMAC: ", inet_ntoa(node->ip_addr.sin_addr));
	print_hwa(node->if_haddr, 6);
	printf("\n");
}

void free_hwa_ip(struct hwa_ip* node) {
	struct hwa_ip* prev = node;

	if(node != NULL)
		node = node->next;
	else
		return;

	if(node == NULL) {
		free(prev);
		return;
	}

	for(;node != NULL; prev = node, node = node->next) {
		free(prev);
	}
}

