# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

BINS = ARP tour test

CFLAGS = -g -Wall -Wextra -Werror -DDEBUG
CFLAGS = -g -Wall -Wextra -Werror -DDEBUG

all: $(BINS)

%: %.c

debug: CFLAGS=-g -Wall -Werror -Wextra -DDEBUG
debug: all

# targets for code coverage using GNU gcovr
cov: CFLAGS=-g -Wall -Werror -Wextra --coverage -DDEBUG
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

test: test.o common.o ARP_util.o
	$(CC) $(CFLAGS) $< common.o ARP_util.o -o $@ 

ARP: ARP.o common.o get_hw_addrs.o ARP_util.o api.o
	$(CC) $(CFLAGS) $^ -o $@

tour: tour.o common.o api.o ping.o api.o
	$(CC) $(CFLAGS) $< ping.o api.o common.o -o $@

clean:
	rm -fr $(BINS) *.o *core* *.gcno *.gcda *.info *.xml coverage

.PHONY: all clean debug cov check
