#include <stdio.h>    /* perror */
#include <sys/mman.h> /* mmap */

#include "dmm.h"

/*
 * Every block, allocated or free, starts with this header. The size field is
 * the number of usable bytes after the header. The next/prev pointers link
 * free blocks into the free list and are unused while a block is allocated.
 */
typedef struct metadata {
  size_t size;
  struct metadata *next;
  struct metadata *prev;
} metadata_t;

#define METADATA_T_ALIGNED (ALIGN(sizeof(metadata_t)))

/* Address just past the end of a block (header + payload). */
#define BLOCK_END(p) ((char *)(p) + METADATA_T_ALIGNED + (p)->size)

/* Head of the free list: free blocks only, sorted by starting address. */
static metadata_t *freelist = NULL;

/* Set once the heap has been reserved, so a full heap is not mistaken for
   an uninitialized one. */
static bool heap_initialized = false;

void *dmalloc(size_t numbytes) {
  if (!heap_initialized && !dmalloc_init()) {
    return NULL;
  }

  if (numbytes == 0) {
    return NULL;
  }

  size_t aligned_size = ALIGN(numbytes);
  if (aligned_size < numbytes) { /* rounding up overflowed */
    return NULL;
  }
  if (aligned_size > MAX_HEAP_SIZE - METADATA_T_ALIGNED) {
    return NULL;
  }

  /* First fit: take the lowest-addressed free block that is large enough. */
  metadata_t *block = freelist;
  while (block && block->size < aligned_size) {
    block = block->next;
  }
  if (!block) {
    return NULL;
  }

  if (block->size >= aligned_size + METADATA_T_ALIGNED + ALIGNMENT) {
    /* Split: the tail of the block becomes a new free block that takes this
       block's place in the free list. Only split when the remainder can hold
       a header plus at least one aligned word, to avoid zero-size blocks. */
    metadata_t *new_block = (metadata_t *)((char *)block + METADATA_T_ALIGNED + aligned_size);
    new_block->size = block->size - aligned_size - METADATA_T_ALIGNED;
    new_block->prev = block->prev;
    new_block->next = block->next;

    if (new_block->next) {
      new_block->next->prev = new_block;
    }
    if (new_block->prev) {
      new_block->prev->next = new_block;
    } else {
      freelist = new_block;
    }
    block->size = aligned_size;
  } else {
    /* Use the whole block: unlink it from the free list. */
    if (block->prev) {
      block->prev->next = block->next;
    } else {
      freelist = block->next;
    }
    if (block->next) {
      block->next->prev = block->prev;
    }
  }

  block->next = NULL;
  block->prev = NULL;

  return (void *)((char *)block + METADATA_T_ALIGNED);
}

void dfree(void *ptr) {
  if (!ptr) {
    return;
  }

  metadata_t *block = (metadata_t *)((char *)ptr - METADATA_T_ALIGNED);

  /* Find the insertion point that keeps the free list sorted by address. */
  metadata_t *current = freelist;
  metadata_t *prev = NULL;
  while (current && current < block) {
    prev = current;
    current = current->next;
  }

  block->prev = prev;
  block->next = current;
  if (current) {
    current->prev = block;
  }
  if (prev) {
    prev->next = block;
  } else {
    freelist = block;
  }

  /* Coalesce with the following block if the two are adjacent in memory. */
  if (block->next && BLOCK_END(block) == (char *)block->next) {
    metadata_t *next_block = block->next;
    block->size += METADATA_T_ALIGNED + next_block->size;
    block->next = next_block->next;
    if (block->next) {
      block->next->prev = block;
    }
  }

  /* Coalesce with the preceding block if the two are adjacent in memory. */
  if (block->prev && BLOCK_END(block->prev) == (char *)block) {
    metadata_t *prev_block = block->prev;
    prev_block->size += METADATA_T_ALIGNED + block->size;
    prev_block->next = block->next;
    if (block->next) {
      block->next->prev = prev_block;
    }
  }
}

/*
 * Reserve the whole heap with one anonymous mmap and place it on the free
 * list as a single large block.
 */
bool dmalloc_init(void) {
  if (heap_initialized) {
    return true;
  }

  size_t max_bytes = ALIGN(MAX_HEAP_SIZE);

  void *region = mmap(NULL, max_bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (region == MAP_FAILED) {
    perror("dmalloc_init: mmap failed");
    return false;
  }

  freelist = (metadata_t *)region;
  freelist->next = NULL;
  freelist->prev = NULL;
  freelist->size = max_bytes - METADATA_T_ALIGNED;
  heap_initialized = true;
  return true;
}
