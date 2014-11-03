CC=clang
CC=gcc
OLDFLAGS = -g -Wall -Werror -Wextra -pedantic -Wno-variadic-macros
LIBS = -lnsl -lresolv -lsocket -lm
LIBS = -lm -pthread

FLAGS = -g -Wall -Werror -Wextra -DHANDIN
all: client server

create_file: FLAGS=-g -Wall -Werror -Wextra -DCREATE_FILE
create_file: all

debug: FLAGS=-g -Wall -Werror -Wextra -DDEBUG
debug: all

color: FLAGS=-g -Wall -Werror -Wextra
color: all

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

xtcp.o: xtcp.c xtcp.h rtt.c rtt.h
	$(CC) $(FLAGS) -c $< -o $@

clean:
	-rm -f  test client server *.o *.tmp* *core*

touch:
	touch *.c *.h

handin:
	 ~cse533/handin 2 makefile client.c client.h server.c server.h common.c common.h config.h debug.h README rtt.c rtt.h unp.h unpifiplus.h xtcp.c xtcp.h get_ifi_info_plus.c

.PHONY: clean all touch handin create_file debug
.SECONDARY:
