CC=gcc
FLAGS=-g

all: client echoc timec

%: %.c

%: %.o
	$(CC) $< -o $@ 

%.o: %.c
	$(CC) -c $< -o $@

clean:
	rm -f client client.o server server.o echoc echoc.o timec timec.o
