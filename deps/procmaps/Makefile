CFLAGS=-O0 -g3
override CFLAGS+=-std=gnu99 -Wall -Werror -DPROCMAPS_TEST=1
procmaps-test: procmaps.c procmaps.h procmaps-test.c
	$(CC) $(CFLAGS) -o $@ procmaps.c procmaps-test.c

clean:
	rm -f procmaps-test test.out

test.expected: procmaps-test
	./test.sh >& $@

check: procmaps-test test.expected
	./test.sh >& test.out
	diff -sq test.out test.expected
