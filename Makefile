CC=gcc
CFLAGS=-Wall -g -std=c2x -fsanitize=address,leak -Wvla -Werror -Wuninitialized -pedantic
LDFLAGS=-lm -lpthread
INCLUDE=-Iinclude -D_XOPEN_SOURCE=700

.PHONY: clean p1tests p2tests

# Required for Part 1 - Make sure it outputs a .o file
# to either objs/ or ./
# In your directory
pkgchk.o: src/chk/pkgchk.c
	$(CC) -c $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS)


pkgmain: src/pkgmain.c src/chk/pkgchk.c src/crypt/sha256.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@

# Required for Part 2 - Make sure it outputs `btide` file
# in your directory ./
btide: src/btide.c src/dyn_array.c src/cli.c src/chk/pkgchk.c \
		src/crypt/sha256.c src/packet.c src/thread_array.c src/peer.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@ 

packet_dump: src/packet.c src/packet_dump.c src/chk/pkgchk.c src/crypt/sha256.c\
			src/dyn_array.c
	$(CC) $^ $(INCLUDE) $(CFLAGS) $(LDFLAGS) -o $@ 

# Alter your build for p1 tests to build unit-tests for your
# merkle tree, use pkgchk to help with what to test for
# as well as some basic functionality
p1tests: pkgmain
	bash p1test.sh

# Alter your build for p2 tests to build IO tests
# for your btide client, construct .in/.out files
# and construct a script to help test your client
# You can opt to constructing a program to
# be the tests instead, however please document
# your testing methods
p2tests: btide packet_dump
	bash p2test.sh

clean:
	rm -f objs/*
	rm -f pkgmain
	rm -f pkgchecker
	rm -f btide
	rm -f packet_dump
    

