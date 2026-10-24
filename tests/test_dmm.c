/*
 * Test suite for the dmalloc / dfree heap allocator.
 *
 * Build and run with:  make test
 *
 * The tests assume the default heap size from the Makefile (1 MB).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dmm.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                   \
  do {                                                                     \
    checks++;                                                              \
    if (!(cond)) {                                                         \
      failures++;                                                          \
      fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);      \
    }                                                                      \
  } while (0)

/* Each block header holds a size and two list pointers (24 bytes on 64-bit). */
#define HEADER_SIZE (3 * sizeof(void *))

/* A request that should fit only in a completely free, fully coalesced heap. */
#define FULL_HEAP_REQUEST (MAX_HEAP_SIZE - 2 * HEADER_SIZE)

/* Free everything in an array of pointers. */
static void free_all(void **ptrs, int n) {
  for (int i = 0; i < n; i++) {
    dfree(ptrs[i]);
    ptrs[i] = NULL;
  }
}

/* After all blocks are freed, the heap should coalesce back into one block. */
static void check_heap_fully_coalesced(const char *context) {
  void *big = dmalloc(FULL_HEAP_REQUEST);
  char msg[160];
  snprintf(msg, sizeof msg, "%s: heap did not coalesce back into one block", context);
  CHECK(big != NULL, msg);
  dfree(big);
}

static void test_basic_alloc_free(void) {
  puts("test_basic_alloc_free");
  char *p = dmalloc(100);
  CHECK(p != NULL, "dmalloc(100) returned NULL");
  memset(p, 0xAB, 100);
  char *q = dmalloc(200);
  CHECK(q != NULL, "dmalloc(200) returned NULL");
  CHECK(q >= p + 100 || q + 200 <= p, "allocations overlap");
  for (int i = 0; i < 100; i++) {
    if ((unsigned char)p[i] != 0xAB) {
      CHECK(0, "second allocation corrupted the first");
      break;
    }
  }
  dfree(p);
  dfree(q);
  check_heap_fully_coalesced("basic");
}

static void test_alignment(void) {
  puts("test_alignment");
  void *ptrs[32];
  for (int i = 0; i < 32; i++) {
    ptrs[i] = dmalloc((size_t)i + 1);
    CHECK(ptrs[i] != NULL, "small allocation failed");
    CHECK(((uintptr_t)ptrs[i] % ALIGNMENT) == 0, "pointer not aligned");
  }
  free_all(ptrs, 32);
  check_heap_fully_coalesced("alignment");
}

static void test_coalescing_orders(void) {
  puts("test_coalescing_orders");
  /* Free three neighbouring blocks in every possible order. */
  int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  for (int o = 0; o < 6; o++) {
    void *b[3];
    for (int i = 0; i < 3; i++) {
      b[i] = dmalloc(1000);
      CHECK(b[i] != NULL, "allocation failed");
    }
    for (int i = 0; i < 3; i++) {
      dfree(b[orders[o][i]]);
    }
    check_heap_fully_coalesced("coalescing order");
  }
}

static void test_reuse_after_free(void) {
  puts("test_reuse_after_free");
  void *a = dmalloc(4096);
  void *b = dmalloc(4096);
  dfree(a);
  void *c = dmalloc(4096);
  CHECK(c == a, "freed block was not reused by first fit");
  dfree(b);
  dfree(c);
  check_heap_fully_coalesced("reuse");
}

static void test_oversized_and_zero(void) {
  puts("test_oversized_and_zero");
  CHECK(dmalloc(MAX_HEAP_SIZE) == NULL, "request larger than heap should fail");
  CHECK(dmalloc(SIZE_MAX) == NULL, "SIZE_MAX request should fail without overflow");
  CHECK(dmalloc(0) == NULL, "dmalloc(0) should return NULL");
  dfree(NULL); /* must not crash */
  check_heap_fully_coalesced("oversized");
}

static void test_exhaustion(void) {
  puts("test_exhaustion");
  /* Take the entire heap, then make sure further requests fail instead of
     silently creating a second heap. */
  void *all = dmalloc(MAX_HEAP_SIZE - HEADER_SIZE);
  CHECK(all != NULL, "could not allocate the whole heap");
  void *extra = dmalloc(64);
  CHECK(extra == NULL, "allocation succeeded after the heap was full");
  if (extra) dfree(extra);
  dfree(all);
  check_heap_fully_coalesced("exhaustion");
}

/* Random allocate / free workload that fills each block with a pattern and
   verifies it is still intact when freed. */
static void test_random_stress(void) {
  puts("test_random_stress");
  enum { SLOTS = 256, ROUNDS = 20000 };
  unsigned char *ptr[SLOTS] = {0};
  size_t len[SLOTS] = {0};
  srand(310);
  for (int r = 0; r < ROUNDS; r++) {
    int i = rand() % SLOTS;
    if (ptr[i]) {
      for (size_t k = 0; k < len[i]; k++) {
        if (ptr[i][k] != (unsigned char)(i ^ k)) {
          CHECK(0, "block contents corrupted");
          break;
        }
      }
      dfree(ptr[i]);
      ptr[i] = NULL;
    } else {
      len[i] = (size_t)(rand() % 2048) + 1;
      ptr[i] = dmalloc(len[i]);
      if (ptr[i]) {
        for (size_t k = 0; k < len[i]; k++) ptr[i][k] = (unsigned char)(i ^ k);
      }
    }
  }
  for (int i = 0; i < SLOTS; i++) dfree(ptr[i]);
  check_heap_fully_coalesced("stress");
}

int main(void) {
  test_basic_alloc_free();
  test_alignment();
  test_coalescing_orders();
  test_reuse_after_free();
  test_oversized_and_zero();
  test_exhaustion();
  test_random_stress();

  printf("\n%d of %d checks passed\n", checks - failures, checks);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
