# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

BINS = ARP tour test

CFLAGS = -g -pthread -Wall -Wextra -Werror -DDEBUG
CFLAGS = -g -pthread -Wall -Wextra -Werror -DDEBUG

all: $(BINS)

%: %.c

debug: CFLAGS=-g -pthread -Wall -Werror -Wextra -DDEBUG
debug: all

# targets for code coverage using GNU gcovr
cov: CFLAGS=-g -pthread -Wall -Werror -Wextra --coverage -DDEBUG
cov: all

check: cov
	./test
	lcov -c -d . -o out.info
	genhtml out.info -o coverage
	gcovr -r . --xml > coverage.xml

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%: %.o
	$(CC) $(CFLAGS) $< -o $@

common.o: common.c common.h api.o
	$(CC) $(CFLAGS) api.o -c $<

test: test.o common.o ARP_util.o api.o
	$(CC) $(CFLAGS) $< api.o common.o ARP_util.o -o $@

ARP: ARP.o common.o get_hw_addrs.o ARP_util.o api.o
	$(CC) $(CFLAGS) $^ -o $@_cse533-11

tour: tour.o common.o api.o ping.o api.o
	$(CC) $(CFLAGS) $< ping.o api.o common.o -o $@_cse533-11

clean:
	rm -fr $(BINS) *.o *core* *.gcno *.gcda *.info *.xml coverage *_cse533-11

.PHONY: all clean debug cov check
