CC=clang
CC=gcc
FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros
LIBS = -lnsl -lresolv -lsocket
LIBS = 

all: client server

%: %.c

%: %.o common.o
	$(CC) $(FLAGS) $(LIBS) $^ -o $@

%.o: %.c %.h
	$(CC) $(FLAGS) -c $< -o $@

common.o: common.c common.h
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f client server *.o

touch:
	touch *.c *.h

handin:
	 ~cse533/handin 2 Makefile client.c client.h server.c server.h 

.PHONY: clean all touch handin
.SECONDARY:
