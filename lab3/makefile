
CC = gcc

LIBS =  /home/courses/cse533/Stevens/unpv13e/libunp.a
LIBS =

FLAGS = -g -O2

all: ODR prhwaddrs


get_hw_addrs.o: get_hw_addrs.c
	$(CC) $(FLAGS) -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	$(CC) $(FLAGS) -c prhwaddrs.c

prhwaddrs: get_hw_addrs.o prhwaddrs.o
	$(CC) -o prhwaddrs prhwaddrs.o get_hw_addrs.o $(LIB)

ODR.o: ODR.c ODR.h debug.h get_hw_addrs.h
	$(CC) $(FLAGS) -c $<

ODR: ODR.o get_hw_addrs.o
	$(CC) $(FLAGS) $< get_hw_addrs.o -o $@

clean:
	rm -f prhwaddrs prhwaddrs.o get_hw_addrs.o ODR.o ODR

