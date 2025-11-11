#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct Block {
    size_t size;
    int free;
    struct Block *next;
} Block;

#define BLOCK_SIZE sizeof(Block)

static void *memory_pool = NULL;
static size_t pool_size = 0;
static Block *free_list = NULL;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initialize memory pool using mmap.
 */
void mem_init(size_t size) {
    pthread_mutex_init(&mem_lock, NULL);

    pool_size = size;

    memory_pool = mmap(NULL, pool_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (memory_pool == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    // first free block in pool
    free_list = (Block *)memory_pool;
    free_list->size = pool_size - BLOCK_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
}

/**
 * First-fit allocation from pool (no malloc allowed).
 */
void *mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);

    Block *current = free_list;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            // split if block is large
            if (current->size >= size + BLOCK_SIZE + 1) {
                Block *new_block = (Block *)((char *)(current + 1) + size);
                new_block->size = current->size - size - BLOCK_SIZE;
                new_block->free = 1;
                new_block->next = current->next;
                current->next = new_block;
                current->size = size;
            }
            current->free = 0;
            pthread_mutex_unlock(&mem_lock);
            return (void *)(current + 1);
        }
        current = current->next;
    }

    pthread_mutex_unlock(&mem_lock);
    printf("mem_alloc: inget ledigt minne\n");
    return NULL;
}

/**
 * Free a block and merge adjacent free blocks.
 */
void mem_free(void *ptr) {
    if (!ptr)
        return;

    pthread_mutex_lock(&mem_lock);

    Block *block = (Block *)ptr - 1;
    block->free = 1;

    // merge neighbors
    Block *curr = free_list;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resize a block (simple version).
 */
void *mem_resize(void *ptr, size_t size) {
    if (!ptr)
        return mem_alloc(size);

    Block *old = (Block *)ptr - 1;
    if (old->size >= size)
        return ptr;

    void *new_ptr = mem_alloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old->size);
        mem_free(ptr);
    }
    return new_ptr;
}

/**
 * Deinitialize memory manager and free mmap region.
 */
void mem_deinit(void) {
    if (memory_pool) {
        munmap(memory_pool, pool_size);
        memory_pool = NULL;
        free_list = NULL;
        pool_size = 0;
    }
    pthread_mutex_destroy(&mem_lock);
}
