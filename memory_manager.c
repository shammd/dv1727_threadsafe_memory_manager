#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct Block {
    size_t size;  // Total block size
    int free;
} Block;  // Footer only

#define BLOCK_SIZE sizeof(Block)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static Block* free_list = NULL;  // Linked list of free blocks (pointers to their starts)
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
    // First free block: starts at memory_pool, size = pool_size - BLOCK_SIZE, footer at end
    free_list = (Block*)memory_pool;
    free_list->size = pool_size - BLOCK_SIZE;
    free_list->free = 1;
    Block* footer = (Block*)((char*)memory_pool + pool_size - BLOCK_SIZE);
    footer->size = pool_size - BLOCK_SIZE;
    footer->free = 1;
    free_list = (Block*)memory_pool;  // free_list points to start of free block
    ((Block*)memory_pool)->next = NULL;  // Add next pointer to Block for free list
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allocates memory using first-fit.
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size = (size + 7) & ~7UL;  // Align
    Block* curr = free_list;
    Block* prev = NULL;
    while (curr) {
        Block* footer = (Block*)((char*)curr + curr->size);
        if (footer->free && footer->size >= size + BLOCK_SIZE) {
            // Allocate here
            void* user_start = (void*)curr;
            size_t remaining = footer->size - size - BLOCK_SIZE;
            if (remaining > BLOCK_SIZE) {
                // Split: new free block after
                Block* new_free = (Block*)((char*)curr + size + BLOCK_SIZE);
                new_free->size = remaining;
                new_free->free = 1;
                Block* new_footer = (Block*)((char*)new_free + remaining);
                new_footer->size = remaining;
                new_footer->free = 1;
                new_free->next = curr->next;
                if (prev) prev->next = new_free;
                else free_list = new_free;
            } else {
                // Remove from free list
                if (prev) prev->next = curr->next;
                else free_list = curr->next;
            }
            // Set footer for allocated block
            Block* alloc_footer = (Block*)((char*)curr + size + BLOCK_SIZE);
            alloc_footer->size = size + BLOCK_SIZE;
            alloc_footer->free = 0;
            pthread_mutex_unlock(&mem_lock);
            return user_start;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

/**
 * Frees a block.
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    Block* footer = (Block*)((char*)ptr + ((Block*)((char*)ptr + *(size_t*)ptr))->size);
    footer->free = 1;
    // Add to free list
    ((Block*)ptr)->next = free_list;
    free_list = (Block*)ptr;
    // Merge with next if adjacent and free
    char* next_start = (char*)ptr + footer->size;
    if (next_start < (char*)memory_pool + pool_size) {
        Block* next_footer = (Block*)(next_start + ((Block*)next_start)->size);
        if (next_footer->free) {
            footer->size += next_footer->size;
            // Remove next from free list
            Block* temp = free_list;
            Block* temp_prev = NULL;
            while (temp && temp != (Block*)next_start) {
                temp_prev = temp;
                temp = temp->next;
            }
            if (temp_prev) temp_prev->next = temp->next;
            else free_list = temp->next;
        }
    }
    // Merge with prev (simplified, iterate free list)
    Block* curr = free_list;
    while (curr) {
        if ((char*)curr + curr->size == (char*)ptr && curr->free) {
            curr->size += footer->size;
            footer = (Block*)((char*)curr + curr->size);
            footer->size = curr->size;
            // Remove ptr from free list
            Block* temp = free_list;
            Block* temp_prev = NULL;
            while (temp && temp != (Block*)ptr) {
                temp_prev = temp;
                temp = temp->next;
            }
            if (temp_prev) temp_prev->next = temp->next;
            else free_list = temp->next;
            break;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Resizes a block.
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    Block* footer = (Block*)((char*)ptr + ((Block*)ptr)->size);
    size_t old_size = footer->size - BLOCK_SIZE;
    if (old_size >= size) return ptr;
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
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
        free_list = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
}