#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>

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
 * Allocate exactly the requested size using mmap.
 * User memory starts at memory_pool (no headers before).
 */
void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);

    pool_size = size;
    memory_pool = mmap(NULL, pool_size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory_pool == MAP_FAILED) {
        perror("mmap failed");
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }

    // Metadata placed at the END of the first block area, not before pool start.
    free_list = (Block *)((char *)memory_pool);
    free_list->size = pool_size;
    free_list->free = 1;
    free_list->next = NULL;

    pthread_mutex_unlock(&mem_lock);
}

/**
 * First-fit allocator inside mmap pool.
 */
void *mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    // Align to 8 bytes
    size = (size + 7) & ~7UL;

    Block *curr = free_list, *prev = NULL;

    while (curr) {
        if (curr->free && curr->size >= size + BLOCK_SIZE) {
            // Split
            char *addr = (char *)curr;
            Block *new_block = (Block *)(addr + BLOCK_SIZE + size);
            new_block->size = curr->size - size - BLOCK_SIZE;
            new_block->free = 1;
            new_block->next = curr->next;

            curr->size = size;
            curr->free = 0;
            curr->next = new_block;

            pthread_mutex_unlock(&mem_lock);
            return addr + BLOCK_SIZE;
        }
        if (curr->free && curr->size >= size) {
            curr->free = 0;
            pthread_mutex_unlock(&mem_lock);
            return (char *)curr + BLOCK_SIZE;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

/**
 * Free a block and merge adjacent free ones.
 */
void mem_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&mem_lock);

    Block *block = (Block *)((char *)ptr - BLOCK_SIZE);
    block->free = 1;

    Block *curr = free_list;
    while (curr && curr->next) {
        char *end = (char *)curr + BLOCK_SIZE + curr->size;
        if (curr->free && curr->next->free && end == (char *)curr->next) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resize an existing block.
 */
void *mem_resize(void *ptr, size_t size) {
    if (!ptr)
        return mem_alloc(size);

    Block *old = (Block *)((char *)ptr - BLOCK_SIZE);
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
 * Release mmap memory.
 */
void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        munmap(memory_pool, pool_size);
        memory_pool = NULL;
        free_list = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}
