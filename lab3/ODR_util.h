#ifndef ODR_UTIL_H
#define ODR_UTIL_H 1

#include "ODR.h"

char *getvmname(char ip[INET_ADDRSTRLEN]);
void print_odr_msg(struct odr_msg *m);

#endif /* ODR_UTIL_H */
