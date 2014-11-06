#include "get_hw_addrs.h"

int
main (int argc, char **argv)
{
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr, straddrbuf[INET6_ADDRSTRLEN];
	int    i, prflag;

	printf("\n");
    hwahead = hwa = Get_hw_addrs();
	for(; hwa != NULL; hwa = hwa->hwa_next) {

		printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
		if ((sa = hwa->ip_addr) != NULL) {
            switch(sa->sa_family) {
                case AF_INET:
                    inet_ntop(AF_INET, &((struct sockaddr_in *) sa)->sin_addr, straddrbuf, INET6_ADDRSTRLEN);
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
			if(hwa->if_haddr[i] != '\0') {
				prflag = 1;
				break;
			}
		} while (++i < IFHWADDRLEN);

		if (prflag) {
			printf("         HW addr = ");
			ptr = hwa->if_haddr;
			i = IFHWADDRLEN;
			do {
				printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
			} while (--i > 0);
		}

		printf("\n         interface index = %d\n\n", hwa->if_index);
	}

	free_hwa_info(hwahead);
	exit(0);
}
