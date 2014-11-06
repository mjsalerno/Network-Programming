# Author Mike Salerno
# Author Scott Harvey

CC = gcc

# Location of Steven's lib on minix
# LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =

FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros

all: ODR


get_hw_addrs.o: get_hw_addrs.c
	$(CC) $(FLAGS) -c get_hw_addrs.c

ODR.o: ODR.c ODR.h debug.h get_hw_addrs.h
	$(CC) $(FLAGS) -c $<

ODR: ODR.o get_hw_addrs.o
	$(CC) $(FLAGS) $< get_hw_addrs.o -o $@

clean:
	rm -f get_hw_addrs.o ODR.o ODR

.PHONY: all clean