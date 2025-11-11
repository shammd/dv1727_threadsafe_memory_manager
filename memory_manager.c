#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct Block {
    size_t size;  // Total size of the block (including footers)
    int free;
    struct Block* next;  // Only used for free blocks' headers
} Block;

#define BLOCK_SIZE sizeof(Block)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static Block* free_list = NULL;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initializes the memory manager with a pool of the given size.
 * Uses malloc for the pool to match test expectations.
 */
void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        pthread_mutex_unlock(&mem_lock);
        return;  // Already initialized
    }
    pool_size = size;
    memory_pool = malloc(pool_size);
    if (!memory_pool) {
        perror("malloc failed");
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }
    // Initialize the first free block: header at start, footer at end
    free_list = (Block*)memory_pool;
    free_list->size = pool_size;
    free_list->free = 1;
    free_list->next = NULL;
    Block* footer = (Block*)((char*)memory_pool + pool_size - BLOCK_SIZE);
    footer->size = pool_size;
    footer->free = 1;
    footer->next = NULL;  // Footer doesn't use next
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allocates memory using first-fit within the pool.
 * User data starts at the beginning of the block (no header before).
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size = (size + 7) & ~7UL;  // Align to 8 bytes
    Block* curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size + 2 * BLOCK_SIZE) {  // Space for user data, header, and footer
            // Split the block
            size_t remaining_size = curr->size - size - 2 * BLOCK_SIZE;
            if (remaining_size > BLOCK_SIZE) {  // Only split if remaining is useful
                // New free block after this allocation
                Block* new_header = (Block*)((char*)curr + BLOCK_SIZE + size + BLOCK_SIZE);
                new_header->size = remaining_size;
                new_header->free = 1;
                new_header->next = curr->next;
                // Footer for new block
                Block* new_footer = (Block*)((char*)new_header + remaining_size - BLOCK_SIZE);
                new_footer->size = remaining_size;
                new_footer->free = 1;
                new_footer->next = NULL;
                // Update current block's footer
                Block* curr_footer = (Block*)((char*)curr + curr->size - BLOCK_SIZE);
                curr_footer->size = size + 2 * BLOCK_SIZE;
                curr_footer->free = 0;
                curr_footer->next = NULL;
                // Update current header
                curr->size = size + 2 * BLOCK_SIZE;
                curr->free = 0;
                curr->next = new_header;
            } else {
                // No split, allocate the whole block
                curr->free = 0;
                Block* curr_footer = (Block*)((char*)curr + curr->size - BLOCK_SIZE);
                curr_footer->free = 0;
            }
            pthread_mutex_unlock(&mem_lock);
            return (char*)curr + BLOCK_SIZE;  // User data starts here
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
    return NULL;  // No space
}

/**
 * Frees a block and merges adjacent free blocks.
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    // Find the footer of the block to free
    Block* footer = (Block*)((char*)ptr - BLOCK_SIZE + ((Block*)((char*)ptr - BLOCK_SIZE))->size - BLOCK_SIZE);
    footer->free = 1;
    // Find the header (start of block)
    Block* header = (Block*)((char*)footer - footer->size + BLOCK_SIZE);
    header->free = 1;
    // Add to free list if not already
    if (free_list != header) {
        header->next = free_list;
        free_list = header;
    }
    // Merge with next free block
    char* end = (char*)header + header->size;
    if ((char*)header->next == end && header->next && header->next->free) {
        header->size += header->next->size;
        header->next = header->next->next;
        // Update footer
        Block* new_footer = (Block*)((char*)header + header->size - BLOCK_SIZE);
        new_footer->size = header->size;
        new_footer->free = 1;
    }
    // Merge with previous (iterate free list)
    Block* prev = NULL;
    Block* curr = free_list;
    while (curr) {
        if ((char*)curr + curr->size == (char*)header && curr->free) {
            curr->size += header->size;
            curr->next = header->next;
            // Update footer
            Block* new_footer = (Block*)((char*)curr + curr->size - BLOCK_SIZE);
            new_footer->size = curr->size;
            new_footer->free = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block (may move if necessary).
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    pthread_mutex_lock(&mem_lock);
    Block* footer = (Block*)((char*)ptr - BLOCK_SIZE + ((Block*)((char*)ptr - BLOCK_SIZE))->size - BLOCK_SIZE);
    size_t old_size = footer->size - 2 * BLOCK_SIZE;
    if (old_size >= size) {
        pthread_mutex_unlock(&mem_lock);
        return ptr;
    }
    pthread_mutex_unlock(&mem_lock);
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
        mem_free(ptr);
    }
    return new_ptr;
}

/**
 * Deinitializes the memory pool.
 */
void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        free(memory_pool);
        memory_pool = NULL;
        free_list = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}
