# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

# Location of Steven's lib on minix
# LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =

FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros -DDEBUG
FLAGS = -g -Wall -Wextra -Werror -DDEBUG

all: ODR server client

%: %.c

%.o: %.c %.h 
	$(CC) $(FLAGS) -c $<

%: %.o ODR_api.o
	$(CC) $(FLAGS) ODR_api.o $< -o $@

ODR_api.o: ODR_api.c ODR_api.h
	$(CC) $(FLAGS) -c ODR_api.c

ODR.o: ODR.c ODR.h debug.h get_hw_addrs.h
	$(CC) $(FLAGS) -c $<

ODR_util.o: ODR_util.c ODR_util.h debug.h
	$(CC) $(FLAGS) -c $<

ODR: ODR.o get_hw_addrs.o ODR_util.o
	$(CC) $(FLAGS) $< get_hw_addrs.o ODR_util.o -o $@

clean:
	rm -f  *.o ODR client server *core* sock_* 

.PHONY: all clean
