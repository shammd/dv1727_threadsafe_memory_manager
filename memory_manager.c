#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct Block {
    size_t size;
    int free;
} Block;  // Only footer now

#define BLOCK_SIZE sizeof(Block)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initializes the memory manager with a pool of the given size.
 */
void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }
    pool_size = size;
    memory_pool = malloc(pool_size);
    if (!memory_pool) {
        perror("malloc failed");
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }
    // Initialize first block as free, footer at end
    Block* footer = (Block*)((char*)memory_pool + pool_size - BLOCK_SIZE);
    footer->size = pool_size;
    footer->free = 1;
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Finds the first free block that fits.
 */
static Block* find_free_block(size_t size) {
    char* curr = (char*)memory_pool;
    while (curr < (char*)memory_pool + pool_size) {
        Block* footer = (Block*)(curr + ((Block*)(curr + *(size_t*)curr - BLOCK_SIZE))->size - BLOCK_SIZE);
        if (footer->free && footer->size >= size + BLOCK_SIZE) {
            return footer;
        }
        curr += footer->size;
    }
    return NULL;
}

/**
 * Allocates memory.
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size = (size + 7) & ~7UL;
    Block* block = find_free_block(size);
    if (!block) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size_t total_size = block->size;
    if (total_size > size + BLOCK_SIZE) {
        // Split
        Block* new_footer = (Block*)((char*)block - total_size + size + BLOCK_SIZE);
        new_footer->size = total_size - size - BLOCK_SIZE;
        new_footer->free = 1;
        block->size = size + BLOCK_SIZE;
    }
    block->free = 0;
    pthread_mutex_unlock(&mem_lock);
    return (char*)block - block->size + BLOCK_SIZE;  // Start of user data
}

/**
 * Frees a block.
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    Block* footer = (Block*)((char*)ptr + *(size_t*)((char*)ptr - sizeof(size_t)) - BLOCK_SIZE);
    footer->free = 1;
    // Merge with next
    char* next_start = (char*)footer + BLOCK_SIZE;
    if (next_start < (char*)memory_pool + pool_size) {
        Block* next_footer = (Block*)(next_start + ((Block*)next_start)->size - BLOCK_SIZE);
        if (next_footer->free) {
            footer->size += next_footer->size;
        }
    }
    // Merge with prev (simplified)
    // ... (implement if needed)
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block.
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    // Simplified: always realloc
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        size_t old_size = *(size_t*)((char*)ptr - sizeof(size_t));
        memcpy(new_ptr, ptr, old_size > size ? size : old_size);
        mem_free(ptr);
    }
    return new_ptr;
}

/**
 * Deinitializes.
 */
void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        free(memory_pool);
        memory_pool = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}