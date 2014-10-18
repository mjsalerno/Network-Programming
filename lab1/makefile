CC=gcc
FLAGS = -g -Wall -Werror -Wextra
LIBS = -lnsl -lresolv -lsocket
LINUX_LIBS = -pthread

all: client echoc timec server

linux: linux-client linux-echoc linux-timec linux-server

%: %.c

%: %.o
	$(CC) $(FLAGS) $(LIBS) $< -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

linux-%: %.o
	$(CC) $(FLAGS) $(LINUX_LIBS) $< -o $@

linux-%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@


clean:
	-rm -f client server echoc timec *.o

touch:
	touch *.c *.h

handin:
	~cse533/handin 1 Makefile client.c const.h echoc.c server.c server.h timec.c

.PHONY: clean all touch linux
.SECONDARY:
