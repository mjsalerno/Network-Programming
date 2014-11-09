# Author Mike Salerno
# Author Scott Harvey

CC = gcc

# Location of Steven's lib on minix
# LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =

FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros
FLAGS = -g -Wall -Werror 
FLAGS = 

all: ODR server client

%: %.c

%.o: %.c %.h 
	$(CC) $(FLAGS) -c $<

%: %.o common.o
	$(CC) $(FLAGS) common.o $< -o $@

common.o: common.c common.h
	$(CC) $(FLAGS) -c common.c

ODR.o: ODR.c ODR.h debug.h get_hw_addrs.h
	$(CC) $(FLAGS) -c $<

ODR: ODR.o get_hw_addrs.o
	$(CC) $(FLAGS) $< get_hw_addrs.o -o $@

clean:
	rm -f  *.o ODR client server

.PHONY: all clean
