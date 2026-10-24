#ifndef DMM_H
#define DMM_H

/*
 * dmm: a dynamic memory manager (malloc/free replacement) for a fixed-size heap.
 *
 * The heap is a single region obtained from the operating system with mmap.
 * Free blocks are kept on an address-ordered, doubly linked free list.
 * Allocation is first fit with block splitting; freeing coalesces a block
 * with its free neighbours in both directions.
 */

#include <stdbool.h>
#include <stddef.h>

/* Total heap size in bytes. Override at build time, e.g. -DMAX_HEAP_SIZE=4096. */
#ifndef MAX_HEAP_SIZE
#define MAX_HEAP_SIZE (1024 * 1024) /* 1 MB */
#endif

/* All returned pointers are aligned to the size of a pointer. */
#define WORD_SIZE 8
#define ALIGNMENT WORD_SIZE

/* Round size up to the nearest multiple of ALIGNMENT. */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1))

/* Reserve the heap. Called automatically by the first dmalloc. */
bool dmalloc_init(void);

/* Allocate numbytes bytes. Returns NULL if numbytes is 0 or no block fits. */
void *dmalloc(size_t numbytes);

/* Release a block returned by dmalloc. dfree(NULL) does nothing. */
void dfree(void *allocptr);

#endif /* DMM_H */
