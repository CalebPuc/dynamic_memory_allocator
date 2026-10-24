# Dynamic Memory Allocator in C
 
A `malloc` / `free` replacement written in C that manages its own heap. It reserves one region of memory from the operating system with `mmap`, then hands out and reclaims pieces of it using an address-ordered, doubly linked free list with first-fit allocation, block splitting, and coalescing in both directions.
 
[![tests](https://github.com/CalebPuc/dynamic_memory_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/CalebPuc/dynamic_memory_allocator/actions/workflows/ci.yml)
 
---
 
## Highlights
 
- **Custom heap** obtained directly from the OS with an anonymous `mmap`, with no calls to the system `malloc`
- **First-fit allocation** that splits large free blocks so the unused tail stays available
- **Bidirectional coalescing** on free: a released block merges with free neighbours before and after it, preventing fragmentation from piling up
- **Address-ordered free list**, which makes finding adjacent neighbours a pointer comparison
- **8-byte alignment** for every returned pointer, with overflow-safe size rounding
- **Test suite** of 103 checks, including a 20,000-operation randomized stress test that verifies no block's contents are ever corrupted
- **Continuous integration** on GitHub Actions with GCC and Clang, plus AddressSanitizer and UndefinedBehaviorSanitizer
---
 
## How it works
 
### Block layout
 
Every block, allocated or free, begins with a 24-byte header:
 
```c
typedef struct metadata {
  size_t size;              // usable bytes after the header
  struct metadata *next;    // next free block (free blocks only)
  struct metadata *prev;    // previous free block (free blocks only)
} metadata_t;
```
 
`dmalloc` returns the address just past the header, and `dfree` steps back by the header size to recover it.
 
```
 heap start                                                           heap end
 ┌────────┬──────────────┬────────┬──────────┬────────┬────────────────────────┐
 │ header │   allocated  │ header │   free   │ header │          free          │
 └────────┴──────────────┴────────┴──────────┴────────┴────────────────────────┘
                              ▲                  ▲
          freelist ───────────┘ ◄──── next/prev ─┘
```
 
### Allocation (`dmalloc`)
 
1. Round the request up to a multiple of 8 bytes, rejecting sizes of 0 and sizes that would overflow or exceed the heap.
2. Walk the free list and take the **first** block large enough.
3. If that block has enough left over for another header plus at least 8 bytes, **split** it: the tail becomes a new free block that takes the original's place in the list. Otherwise hand out the whole block and unlink it.
### Freeing (`dfree`)
 
1. Insert the block back into the free list at the position that keeps the list sorted by address.
2. If the block ends exactly where the next free block begins, **merge** them.
3. If the previous free block ends exactly where this block begins, **merge** again.
Because the list is sorted by address, a block's physical neighbours are also its list neighbours, so each merge check is a single comparison.
 
---
 
## Usage
 
```c
#include "dmm.h"
 
int main(void) {
  int *values = dmalloc(100 * sizeof(int));
  if (!values) return 1;
 
  for (int i = 0; i < 100; i++) values[i] = i * i;
 
  dfree(values);
  return 0;
}
```
 
The heap is created automatically on the first call to `dmalloc`. Its size defaults to 1 MB and can be changed at compile time with `-DMAX_HEAP_SIZE=<bytes>`.
 
---
 
## Building and testing
 
Requires a C compiler and `make` on Linux or macOS.
 
```sh
make test        # build and run the test suite
make sanitize    # run the tests under AddressSanitizer and UBSan
make HEAP=4096 test   # run with a different heap size
```
 
The test suite covers:
 
| Test | What it checks |
|---|---|
| Basic allocation | Blocks don't overlap and writing one doesn't corrupt another |
| Alignment | Every returned pointer is 8-byte aligned, for sizes 1–32 |
| Coalescing | Three adjacent blocks freed in all six possible orders always merge back into one |
| Reuse | A freed block is handed out again by the next request that fits |
| Edge cases | Zero-byte, oversized, and `SIZE_MAX` requests fail cleanly; `dfree(NULL)` is safe |
| Exhaustion | Once the heap is full, requests fail instead of reserving more memory |
| Random stress | 20,000 random allocations and frees with content verification, ending fully coalesced |
 
---
 
## Project structure
 
```
src/dmm.h          public interface and configuration
src/dmm.c          allocator implementation
tests/test_dmm.c   test suite
Makefile           build, test, and sanitizer targets
.github/workflows  continuous integration
```
 
---
 
## Limitations and possible improvements
 
- **Linear-time operations:** both allocation and freeing walk the free list, so they slow down as the list grows. Segregated free lists (one list per size range) would make allocation close to constant time.
- **No boundary tags:** blocks have headers but no footers, so `dfree` has to search the list for its neighbours. Adding a footer to each block would allow constant-time coalescing.
- **First fit:** fast and simple, but can leave small unusable fragments at the front of the heap. Best fit or next fit trade speed for different fragmentation behaviour.
- **Fixed heap:** the heap is reserved once and never grows. Requesting additional regions from the OS when it fills would remove the size limit.
- **No misuse detection:** freeing the same pointer twice, or a pointer that didn't come from `dmalloc`, corrupts the free list. A magic number in each header would let `dfree` catch these mistakes.
- **Not thread-safe:** concurrent calls would need a lock or per-thread heaps.
---
 
## Author
 
**Caleb Pucylowski**
[GitHub](https://github.com/CalebPuc) · [LinkedIn](https://www.linkedin.com/in/caleb-pucylowski)
 
