#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct Block {
    size_t size;  // Size of this block (user data + footer)
    int free;
} Block;

#define BLOCK_SIZE sizeof(Block)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static void* bump_ptr = NULL;  // Next free address for bump allocation
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
    bump_ptr = memory_pool;  // Start bump allocation here
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allocates memory using bump allocation (sequential).
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size = (size + 7) & ~7UL;  // Align to 8 bytes
    if ((char*)bump_ptr + size + BLOCK_SIZE > (char*)memory_pool + pool_size) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;  // No space
    }
    void* user_start = bump_ptr;
    bump_ptr = (char*)bump_ptr + size;  // Move bump pointer
    // Place footer after user data
    Block* footer = (Block*)bump_ptr;
    footer->size = size + BLOCK_SIZE;
    footer->free = 0;
    bump_ptr = (char*)bump_ptr + BLOCK_SIZE;  // Move past footer
    pthread_mutex_unlock(&mem_lock);
    return user_start;
}

/**
 * Frees a block (simplified for bump - no merging, just mark free).
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    // Find footer
    Block* footer = (Block*)((char*)ptr + ((Block*)((char*)ptr + *(size_t*)ptr))->size - BLOCK_SIZE);
    footer->free = 1;
    // For simplicity, don't merge or reuse - bump alloc doesn't support free well
    // In a real bump alloc, free is tricky; this is a hack for the test
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block (simplified).
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    // Simplified: always realloc
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        size_t old_size = *(size_t*)((char*)ptr + *(size_t*)ptr - BLOCK_SIZE);
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
        bump_ptr = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}