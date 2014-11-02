CC=clang
CC=gcc
FLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros -DDEBUG
LIBS = -lnsl -lresolv -lsocket -lm -lrt
LIBS = -lm -pthread

all: client server

%: %.c

%: %.o

%: %.o common.o xtcp.o get_ifi_info_plus.o rtt.o
	$(CC) $(FLAGS) $^ $(LIBS) -o $@

%.o: %.c %.h debug.h
	$(CC) $(FLAGS) -c $< -o $@

common.o: common.c common.h unp.h get_ifi_info_plus.o
	$(CC) $(FLAGS) -c $< -o $@

get_ifi_info_plus.o: get_ifi_info_plus.c config.h unp.h unpifiplus.h
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f  test client server *.o *.tmp* *core*

touch:
	touch *.c *.h

handin:
	 ~cse533/handin 2 makefile client.c client.h server.c server.h common.c common.h config.h debug.h README rtt.c rtt.h unp.h unpifiplus.h xtcp.c xtcp.h get_ifi_info_plus.c

.PHONY: clean all touch handin
.SECONDARY:
