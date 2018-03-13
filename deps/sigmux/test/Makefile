CPPFLAGS+=-Wall -Werror
#CFLAGS+=-g3 -O3 -march=native
CFLAGS+=-g3 -O3 
override CPPFLAGS+=-I..
override CFLAGS+=-std=gnu99 -pthread

VPATH=.:..
HEADERS=../phaser.h ../sigmux.h

%.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) -m32 $(CFLAGS) -c -o $@ -I.. $(filter %.c,$^)

%_64.o: %.c $(HEADERS)
	$(CC) $(CPPFLAGS) -m64 $(CFLAGS) -c -o $@ -I.. $(filter %.c,$^)

phaser-test: phaser.o phaser-test.o
	$(CC) $(CFLAGS) -m32 -o $@ $^

phaser-test-64: phaser_64.o phaser-test_64.o
	$(CC) $(CFLAGS) -m64 -o $@ $^

sigmux.a: phaser.o sigmux.o
	rm -f $@
	$(AR) rcs $@ $^

sigmux_64.a: phaser_64.o sigmux_64.o
	rm -f $@
	$(AR) rcs $@ $^

sigmux-test: sigmux-test.o sigmux.a
	$(CC) $(CFLAGS) -m32 -o $@ $^

sigmux-test-64: sigmux-test_64.o sigmux_64.a
	$(CC) $(CFLAGS) -m64 -o $@ $^

clean:
	rm -f phaser-test phaser-test-64 sigmux-test sigmux-test-64 *.o *.a

all: phaser-test sigmux-test

all64: phaser-test-64 sigmux-test-64

.DEFAULT_GOAL=all
