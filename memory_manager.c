#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct Block {
    size_t size;
    int free;
    struct Block* next;
} Block;

#define BLOCK_SIZE sizeof(Block)

static void* memory_pool = NULL;
static size_t pool_size = 0;
static Block* free_list = NULL;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initierar minneshanteraren med en pool av given storlek.
 * Använder malloc för poolen (matchar testförväntningar).
 */
void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        pthread_mutex_unlock(&mem_lock);
        return; // Redan initierad
    }
    pool_size = size;
    memory_pool = malloc(pool_size);
    if (!memory_pool) {
        perror("malloc failed");
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }
    free_list = (Block*)memory_pool;
    free_list->size = pool_size - BLOCK_SIZE; // Reservera plats för header
    free_list->free = 1;
    free_list->next = NULL;
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Allokerar minne med first-fit inom poolen.
 */
void* mem_alloc(size_t size) {
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool || size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }
    size = (size + 7) & ~7UL; // Align to 8 bytes
    Block* curr = free_list;
    Block* prev = NULL;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size > size + BLOCK_SIZE) {
                // Split block
                Block* new_block = (Block*)((char*)curr + BLOCK_SIZE + size);
                new_block->size = curr->size - size - BLOCK_SIZE;
                new_block->free = 1;
                new_block->next = curr->next;
                curr->next = new_block;
            }
            curr->size = size;
            curr->free = 0;
            pthread_mutex_unlock(&mem_lock);
            return (char*)curr + BLOCK_SIZE;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
    return NULL; // Ingen ledig plats
}

/**
 * Frigör ett block och mergar angränsande fria block.
 */
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);
    Block* block = (Block*)((char*)ptr - BLOCK_SIZE);
    block->free = 1;
    // Merge med nästa fria block
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
    // Merge med föregående (iterera genom listan)
    Block* curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next == block && curr->next->free) {
            curr->size += BLOCK_SIZE + block->size;
            curr->next = block->next;
            break;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&mem_lock);
}

/**
 * Ändrar storlek på ett block (kan flytta om nödvändigt).
 */
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    pthread_mutex_lock(&mem_lock);
    Block* old_block = (Block*)((char*)ptr - BLOCK_SIZE);
    if (old_block->size >= size) {
        pthread_mutex_unlock(&mem_lock);
        return ptr;
    }
    pthread_mutex_unlock(&mem_lock);
    void* new_ptr = mem_alloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_block->size);
        mem_free(ptr);
    }
    return new_ptr;
}

/**
 * Frigör poolen.
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