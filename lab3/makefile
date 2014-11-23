# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

# Location of Steven's lib on minix
# LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =

FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros -DDEBUG
FLAGS = -g -Wall -Wextra -Werror

all: ODR server client

%: %.c

debug: FLAGS=-g -Wall -Werror -Wextra -DDEBUG
debug: all

%.o: %.c %.h
	$(CC) $(FLAGS) -c $<

%: %.o ODR_api.o
	$(CC) $(FLAGS) ODR_api.o $< -o $@_cse533-8

ODR_api.o: ODR_api.c ODR_api.h
	$(CC) $(FLAGS) -c ODR_api.c

ODR.o: ODR.c ODR.h ODR_util.h ODR_api.h debug.h get_hw_addrs.h
	$(CC) $(FLAGS) -c $<

ODR_util.o: ODR_util.c ODR_util.h debug.h
	$(CC) $(FLAGS) -c $<

ODR: ODR.o get_hw_addrs.o ODR_util.o
	$(CC) $(FLAGS) $< get_hw_addrs.o ODR_util.o -o $@_cse533-8

clean:
	rm -f  *.o *core* sock_* *_cse533-8 /tmp/cse533-8_*

.PHONY: all clean
