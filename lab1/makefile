CC=gcc
FLAGS = -g -Wall -Werror -Wextra
#LIBS = -pthread
LIBS = -lnsl -lresolv -lsocket

all: client echoc timec server

%: %.c

%: %.o
	$(CC) $(FLAGS) $(LIBS) $< -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f client server echoc timec *.o

touch:
	touch *.c *.h

handin:
	 ~cse533/handin 1 Makefile client.c const.h echoc.c server.c server.h timec.c

.PHONY: clean all touch
.SECONDARY:
