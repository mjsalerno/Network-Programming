# Author Mike Salerno
# Author Scott Harvey
CC = clang
CC = gcc

FLAGS = -g -Wall -Wextra -Werror -DDEBUG

all: test ARP tour

%: %.c

debug: FLAGS=-g -Wall -Werror -Wextra -DDEBUG
debug: all

cov: FLAGS=-g -Wall -Werror -Wextra --coverage -DDEBUG
cov: all

check: cov
	./test
	lcov -c -d . -o out.info
	genhtml out.info -o coverage
	gcovr -r . --xml > coverage.xml

%.o: %.c %.h
	$(CC) $(FLAGS) -c $<

%: %.o
	$(CC) $(FLAGS) $< -o $@

test: test.o common.o
	$(CC) $(FLAGS) $< common.o -o $@ 

ARP: ARP.o common.o get_hw_addrs.o
	$(CC) $(FLAGS) $< common.o get_hw_addrs.o -o $@

tour: tour.o common.o
	$(CC) $(FLAGS) $< common.o -o $@

clean:
	rm -fr  *.o *core* test *.gcno *.gcda *.info *.xml coverage

.PHONY: all clean debug cov check
