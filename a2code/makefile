
CC = gcc

LIBS =  -lsocket
LIBS =

FLAGS =  -g -O2
CFLAGS = ${FLAGS} 

all: get_ifi_info_plus.o prifinfo_plus.o
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}


get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c

clean:
	rm prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o

