CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pedantic
CFLAGS  += -D_DEFAULT_SOURCE -Isrc

# Heap size used by the tests (bytes).
HEAP    ?= 1048576
CFLAGS  += -DMAX_HEAP_SIZE=$(HEAP)

SRC     := src/dmm.c
TEST    := tests/test_dmm.c

.PHONY: all test sanitize clean

all: test_dmm

test_dmm: $(SRC) $(TEST) src/dmm.h
	$(CC) $(CFLAGS) $(SRC) $(TEST) -o $@

test: test_dmm
	./test_dmm

# Run the tests with AddressSanitizer and UndefinedBehaviorSanitizer.
sanitize: $(SRC) $(TEST) src/dmm.h
	$(CC) $(CFLAGS) -g -O1 -fsanitize=address,undefined $(SRC) $(TEST) -o test_dmm_sanitize
	./test_dmm_sanitize

clean:
	rm -f test_dmm test_dmm_sanitize
