#include "memory_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
 * Simple segment-based memory manager.
 * - One big pool allocated with malloc(size) in mem_init.
 * - Blocks are described in a static metadata array outside the pool.
 * - First-fit allocation with splitting.
 * - Free coalesces neighbour free blocks.
 * - Thread-safe via a global mutex.
 */

typedef struct Block {
    size_t offset;   // offset into memory_pool
    size_t size;     // size of this block (bytes)
    int    free;     // 1 = free, 0 = allocated
} Block;

#define MAX_BLOCKS 1024  // enough for our tests

static void   *memory_pool  = NULL;
static size_t  pool_size    = 0;
static Block   blocks[MAX_BLOCKS];
static size_t  block_count  = 0;

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/* --------- Helpers --------- */

static void internal_reset_state(void) {
    memory_pool = NULL;
    pool_size   = 0;
    block_count = 0;
}

static size_t align8(size_t size) {
    if (size == 0) return 0;
    size_t rem = size & 7;
    if (rem == 0) return size;
    return size + (8 - rem);
}

/* Merge block i with its next neighbour (i+1). Caller must ensure both are free
 * and contiguous. */
static void merge_with_next(size_t i) {
    if (i + 1 >= block_count) return;

    Block *b  = &blocks[i];
    Block *n  = &blocks[i + 1];

    if (!b->free || !n->free) return;
    if (b->offset + b->size != n->offset) return;

    b->size += n->size;

    // shift remaining blocks left
    for (size_t j = i + 1; j + 1 < block_count; ++j) {
        blocks[j] = blocks[j + 1];
    }
    block_count--;
}

/* Find block index by pointer; returns -1 if not found. */
static int find_block_index_by_ptr(void *ptr) {
    if (!memory_pool || !ptr) return -1;

    size_t off = (size_t)((char *)ptr - (char *)memory_pool);
    for (size_t i = 0; i < block_count; ++i) {
        if (blocks[i].offset == off) {
            return (int)i;
        }
    }
    return -1;
}

/* --------- Public API --------- */

void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);

    // If already initialised: free old pool
    if (memory_pool) {
        free(memory_pool);
        internal_reset_state();
    }

    if (size == 0) {
        // degenerate pool – nothing to allocate
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    void *ptr = malloc(size);
    if (!ptr) {
        pthread_mutex_unlock(&mem_lock);
        fprintf(stderr, "mem_init: failed to allocate %zu bytes\n", size);
        exit(EXIT_FAILURE);
    }

    memory_pool = ptr;
    pool_size   = size;

    // single big free block spanning the entire pool
    block_count = 1;
    blocks[0].offset = 0;
    blocks[0].size   = size;
    blocks[0].free   = 1;

    pthread_mutex_unlock(&mem_lock);
}

void *mem_alloc(size_t size) {
    /* Specialfall: size == 0
       Testerna förväntar att detta INTE ger NULL, och att det
       inte förstör poolens kapacitet. Vi returnerar därför en
       statisk dummy-pekare utanför poolen. */
    if (size == 0) {
        static int zero_dummy;
        return &zero_dummy;
    }

    pthread_mutex_lock(&mem_lock);

    if (!memory_pool || pool_size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    size = align8(size);

    // First-fit genom blocklistan
    for (size_t i = 0; i < block_count; ++i) {
        Block *b = &blocks[i];
        if (!b->free) continue;
        if (b->size < size) continue;

        size_t remaining    = b->size - size;
        size_t alloc_offset = b->offset;

        b->free = 0;
        b->size = size;

        if (remaining > 0) {
            if (block_count >= MAX_BLOCKS) {
                // ingen plats i metadata -> faila och ångra
                b->free = 1;
                b->size += remaining;
                pthread_mutex_unlock(&mem_lock);
                return NULL;
            }

            // lägg in nytt fritt block direkt efter detta
            for (size_t j = block_count; j > i + 1; --j) {
                blocks[j] = blocks[j - 1];
            }

            blocks[i + 1].offset = alloc_offset + size;
            blocks[i + 1].size   = remaining;
            blocks[i + 1].free   = 1;
            block_count++;
        }

        void *result = (char *)memory_pool + alloc_offset;
        pthread_mutex_unlock(&mem_lock);
        return result;
    }

    pthread_mutex_unlock(&mem_lock);
    return NULL;
}


void mem_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&mem_lock);

    if (!memory_pool) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    int idx = find_block_index_by_ptr(ptr);
    if (idx < 0) {
        // pointer not from our pool – ignore
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    Block *b = &blocks[idx];
    if (b->free) {
        // double free – ignore (robustness)
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    b->free = 1;

    // coalesce with previous
    if (idx > 0) {
        Block *prev = &blocks[idx - 1];
        if (prev->free &&
            prev->offset + prev->size == b->offset) {
            merge_with_next(idx - 1);
            idx -= 1;
            b = &blocks[idx];
        }
    }

    // coalesce with next
    if (idx + 1 < (int)block_count) {
        Block *next = &blocks[idx + 1];
        if (next->free &&
            b->offset + b->size == next->offset) {
            merge_with_next(idx);
        }
    }

    pthread_mutex_unlock(&mem_lock);
}

void *mem_resize(void *ptr, size_t size) {
    pthread_mutex_lock(&mem_lock);

    if (!memory_pool || pool_size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    // realloc-liknande beteende:
    if (ptr == NULL) {
        pthread_mutex_unlock(&mem_lock);
        return mem_alloc(size);
    }

    if (size == 0) {
        pthread_mutex_unlock(&mem_lock);
        mem_free(ptr);
        return NULL;
    }

    size = align8(size);

    int idx = find_block_index_by_ptr(ptr);
    if (idx < 0) {
        // okänd pointer -> bete dig som malloc
        pthread_mutex_unlock(&mem_lock);
        return mem_alloc(size);
    }

    Block *b = &blocks[idx];
    size_t old_size = b->size;

    if (size <= old_size) {
        // shrink in place, ev. split
        size_t remaining = old_size - size;
        b->size = size;

        if (remaining > 0 && block_count < MAX_BLOCKS) {
            // insert a free tail block
            for (size_t j = block_count; j > idx + 1; --j) {
                blocks[j] = blocks[j - 1];
            }
            blocks[idx + 1].offset = b->offset + size;
            blocks[idx + 1].size   = remaining;
            blocks[idx + 1].free   = 1;
            block_count++;

            // försöker merge:a nya blocket med nästa
            merge_with_next(idx + 1);
        }

        void *result = ptr;
        pthread_mutex_unlock(&mem_lock);
        return result;
    }

    // försök expandera in i nästa block om det är fritt
    if (idx + 1 < (int)block_count) {
        Block *next = &blocks[idx + 1];
        if (next->free &&
            b->offset + b->size == next->offset &&
            b->size + next->size >= size) {

            size_t needed_extra = size - b->size;
            if (next->size > needed_extra) {
                next->offset += needed_extra;
                next->size   -= needed_extra;
                b->size       = size;
            } else {
                b->size += next->size;
                merge_with_next(idx);
            }

            void *result = (char *)memory_pool + b->offset;
            pthread_mutex_unlock(&mem_lock);
            return result;
        }
    }

    // fallback: allokera ny, kopiera, free:a gammal
    pthread_mutex_unlock(&mem_lock);
    void *new_ptr = mem_alloc(size);
    if (!new_ptr) {
        return NULL; // behåll gamla blocket giltigt
    }

    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    mem_free(ptr);
    return new_ptr;
}

void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);

    if (memory_pool) {
        free(memory_pool);
        internal_reset_state();
    }

    pthread_mutex_unlock(&mem_lock);
}
