# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

BINS = ARP tour test

FLAGS = -g -Wall -Wextra -Werror -DDEBUG
FLAGS = -g -Wall -Wextra -Werror -DDEBUG

all: $(BINS)

%: %.c

debug: FLAGS=-g -Wall -Werror -Wextra -DDEBUG
debug: all

# targets for code coverage using GNU gcovr
cov: FLAGS=-g -Wall -Werror -Wextra --coverage -DDEBUG
cov: all

check: cov
	./test
	lcov -c -d . -o out.info
	genhtml out.info -o coverage
	gcovr -r . --xml > coverage.xml

# for build .o's that don't have a .h (ARP_util.o)
%.o: %.c
	$(CC) $(FLAGS) -c $<

%.o: %.c %.h
	$(CC) $(FLAGS) -c $<

%: %.o
	$(CC) $(FLAGS) $< -o $@

test: test.o common.o
	$(CC) $(FLAGS) $< common.o -o $@ 

ARP: ARP.o common.o get_hw_addrs.o ARP_util.o api.o
	$(CC) $(FLAGS) $^ -o $@

tour: tour.o common.o api.o
	$(CC) $(FLAGS) $< common.o -o $@

clean:
	rm -fr $(BINS) *.o *core* *.gcno *.gcda *.info *.xml coverage

.PHONY: all clean debug cov check
