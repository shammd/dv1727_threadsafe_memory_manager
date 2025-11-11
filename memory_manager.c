#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
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
 * Initialize memory manager — allocate EXACTLY requested size with mmap().
 * The user-visible memory starts at memory_pool (no metadata before it).
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

    // Place metadata *after* the visible start, so user data starts at memory_pool
    free_list = (Block *)((char *)memory_pool);
    free_list->size = pool_size - BLOCK_SIZE;
    free_list->free = 1;
    free_list->next = NULL;

    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allocate memory within our mmap pool (first-fit).
 */
void *mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);

    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    // Align size to 8 bytes
    size = (size + 7) & ~7UL;
    Block *current = free_list;

    while (current) {
        if (current->free && current->size >= size) {
            size_t remaining = current->size - size;

            if (remaining >= BLOCK_SIZE + 8) {
                Block *new_block = (Block *)((char *)current + BLOCK_SIZE + size);
                new_block->size = remaining - BLOCK_SIZE;
                new_block->free = 1;
                new_block->next = current->next;
                current->next = new_block;
                current->size = size;
            }

            current->free = 0;
            pthread_mutex_unlock(&mem_lock);
            return (void *)((char *)current + BLOCK_SIZE); // user data starts *after* metadata
        }
        current = current->next;
    }

    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

/**
 * Frees a block and merges adjacent free areas.
 */
void mem_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&mem_lock);

    Block *block = (Block *)((char *)ptr - BLOCK_SIZE);
    block->free = 1;

    Block *curr = free_list;
    while (curr && curr->next) {
        char *curr_end = (char *)curr + BLOCK_SIZE + curr->size;
        if (curr->free && curr->next->free && curr_end == (char *)curr->next) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resize a block.
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
 * Deinitialize memory manager.
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
