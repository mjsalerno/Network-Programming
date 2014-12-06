# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

BINS = ARP_cse533-11 tour_cse533-11

CFLAGS = -g -pthread -Wall -Wextra -Werror  -DHANDIN

all: $(BINS)

%: %.c

debug: CFLAGS=-g -pthread -Wall -Werror -Wextra -DDEBUG
debug: all

color: FLAGS=-g -Wall -Werror -Wextra
color: all

# targets for code coverage using GNU gcovr
cov: CFLAGS=-g -pthread -Wall -Werror -Wextra --coverage -DDEBUG
cov: all test

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
	$(CC) $(CFLAGS) -c $<

test: test.o common.o ARP_util.o api.o
	$(CC) $(CFLAGS) $^ -o $@

ARP_cse533-11: ARP.o common.o get_hw_addrs.o ARP_util.o api.o
	$(CC) $(CFLAGS) $^ -o $@

tour_cse533-11: tour.o common.o api.o ping.o api.o
	$(CC) $(CFLAGS) $< ping.o api.o common.o -lpthread -o $@

clean:
	rm -fr $(BINS) *.o *core* *.gcno *.gcda *.info *.xml coverage test *_cse533-11

.PHONY: all clean debug cov check
