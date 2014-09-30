CC=gcc
FLAGS=-g

all: client echoc timec

%: %.c

%: %.o
	$(CC) $(FLAGS) $< -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f client client.o server server.o echoc echoc.o timec timec.o

touch:
	touch *.c *.h

.PHONY: clean all touch
.SECONDARY:
