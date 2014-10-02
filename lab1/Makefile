CC=gcc
FLAGS = -g -Wall -Werror -Wextra -pedantic
LIBS = -pthread
LIBS = -lnsl -lresolv -lsocket

all: client echoc timec server

%: %.c

%: %.o %.h
	$(CC) $(FLAGS) $(LIBS) $(LIBSS) $< -o $@

%.o: %.c
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f client server echoc timec *.o

touch:
	touch *.c *.h

.PHONY: clean all touch
.SECONDARY:
