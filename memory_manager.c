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
 * Initializes the memory manager with a given pool size using mmap().
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

    // Set up the first free block
    free_list = (Block *)memory_pool;
    free_list->size = pool_size - BLOCK_SIZE;
    free_list->free = 1;
    free_list->next = NULL;
}

/**
 * Allocates a block of memory from the custom pool.
 * Uses a simple first-fit algorithm.
 */
void *mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);

    Block *current = free_list;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            // If the block is big enough to split
            if (current->size > size + BLOCK_SIZE) {
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
 * Frees a previously allocated block and merges adjacent free blocks.
 */
void mem_free(void *block) {
    if (!block)
        return;

    pthread_mutex_lock(&mem_lock);

    Block *curr = (Block *)block - 1;
    curr->free = 1;

    // Merge adjacent free blocks
    Block *temp = free_list;
    while (temp) {
        if (temp->free && temp->next && temp->next->free) {
            temp->size += BLOCK_SIZE + temp->next->size;
            temp->next = temp->next->next;
            continue;
        }
        temp = temp->next;
    }

    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block (naive version: allocates new memory, copies data, frees old).
 */
void *mem_resize(void *block, size_t size) {
    if (!block)
        return mem_alloc(size);

    Block *old_block = (Block *)block - 1;
    if (old_block->size >= size)
        return block;

    void *new_block = mem_alloc(size);
    if (new_block) {
        memcpy(new_block, block, old_block->size);
        mem_free(block);
    }
    return new_block;
}

/**
 * Deinitializes the memory pool (munmap).
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
