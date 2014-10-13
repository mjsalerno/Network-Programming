CC=gcc
CC=clang
FLAGS = -g -Wall -Werror -Wextra
LIBS = 
#LIBS = -lnsl -lresolv -lsocket

all: client server

%: %.c

%: %.o
	$(CC) $(FLAGS) $(LIBS) $< -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f client server *.o

touch:
	touch *.c *.h

handin:
	 ~cse533/handin 2 Makefile client.c client.h server.c server.h 

.PHONY: clean all touch handin
.SECONDARY:
