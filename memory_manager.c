#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define BLOCK_SIZE 512  // Fixed block size for simplicity
#define NUM_BLOCKS (6000 / BLOCK_SIZE)  // 11 blocks (6000 / 512 = 11.718, ignore rest)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static int free_blocks[NUM_BLOCKS];  // 1 = free, 0 = allocated
static int next_free = 0;  // Next free block index
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
    for (int i = 0; i < NUM_BLOCKS; i++) {
        free_blocks[i] = 1;  // All free
    }
    next_free = 0;
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allocates a fixed 512-byte block.
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0 || size > BLOCK_SIZE) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;  // Only support 512 bytes
    }
    if (next_free >= NUM_BLOCKS) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;  // No free blocks
    }
    int block_index = next_free;
    free_blocks[block_index] = 0;
    // Find next free
    next_free++;
    while (next_free < NUM_BLOCKS && !free_blocks[next_free]) {
        next_free++;
    }
    pthread_mutex_unlock(&mem_lock);
    return (char*)memory_pool + block_index * BLOCK_SIZE;  // Start at base + index * 512
}

/**
 * Frees a block (mark as free).
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    int block_index = ((char*)ptr - (char*)memory_pool) / BLOCK_SIZE;
    if (block_index >= 0 && block_index < NUM_BLOCKS) {
        free_blocks[block_index] = 1;
        if (block_index < next_free) next_free = block_index;  // Update next_free
    }
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block (simplified).
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    if (size <= BLOCK_SIZE) return ptr;  // Can't resize larger
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, BLOCK_SIZE);
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
        next_free = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}