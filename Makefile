CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all test clean
all: bin/shelf-steam bin/parallel-hashing bin/raid5

bin:
	mkdir -p bin

bin/shelf-steam: shell/shelf-steam.c | bin
	$(CC) $(CFLAGS) -o $@ $<

bin/parallel-hashing: parallel-hashing/hash.c parallel-hashing/hash_functions.c parallel-hashing/hash_functions.h | bin
	$(CC) $(CFLAGS) -pthread -o $@ parallel-hashing/hash.c parallel-hashing/hash_functions.c -lcrypto

bin/raid5: raid5/raid5.c | bin
	$(CC) $(CFLAGS) -o $@ $<

test: all
	python3 tests/test.py

clean:
	rm -rf bin

.PHONY: demo
demo: all
	sh demo/run.sh
