#include "memory_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// Strukturen som beskriver ett block i minnespoolen
typedef struct Block {
    size_t size;          // Storlek på blocket
    int free;             // 1 = ledigt och 0 = upptaget
    struct Block* next;   // Pekare till nästa block
} Block;

static void* memory_pool = NULL;   // Själva minnespoolen
static size_t pool_size = 0;       // Total storlek
static Block* free_list = NULL;    // Start på fria block
static pthread_mutex_t mem_lock;   // Lås för trådsäkerhet

#define BLOCK_SIZE sizeof(Block)

// Initierar minneshanteraren med en pool av angiven storlek
void mem_init(size_t size) {
    if (memory_pool != NULL) {
        fprintf(stderr, "Minneshanteraren är redan initierad!\n");
        return;
    }

    memory_pool = malloc(size);
    if (!memory_pool) {
        fprintf(stderr, "Kunde inte allokera minne!\n");
        exit(EXIT_FAILURE);
    }

    pool_size = size;
    free_list = (Block*)memory_pool;
    free_list->size = size - BLOCK_SIZE;
    free_list->free = 1;
    free_list->next = NULL;

    pthread_mutex_init(&mem_lock, NULL);
}

// Hittar första lediga block som räcker (First-fit)
static Block* find_free_block(size_t size) {
    Block* current = free_list;
    while (current != NULL) {
        if (current->free && current->size >= size)
            return current;
        current = current->next;
    }
    return NULL;
}

// Delar upp ett block om det är större än nödvändigt
static void split_block(Block* block, size_t size) {
    if (block->size <= size + BLOCK_SIZE)
        return;

    Block* new_block = (Block*)((char*)block + BLOCK_SIZE + size);
    new_block->size = block->size - size - BLOCK_SIZE;
    new_block->free = 1;
    new_block->next = block->next;

    block->size = size;
    block->next = new_block;
}

// Allokerar minne från poolen
void* mem_alloc(size_t size) {
    if (size == 0) return NULL;
    pthread_mutex_lock(&mem_lock);

    Block* block = find_free_block(size);
    if (!block) {
        pthread_mutex_unlock(&mem_lock);
        fprintf(stderr, "mem_alloc: inget ledigt minne\n");
        return NULL;
    }

    split_block(block, size);
    block->free = 0;

    pthread_mutex_unlock(&mem_lock);
    return (char*)block + BLOCK_SIZE;
}

// Slår ihop fria block efter frigöring
static void merge_free_blocks() {
    Block* current = free_list;
    while (current && current->next) {
        Block* next = current->next;
        if (current->free && next->free) {
            current->size += BLOCK_SIZE + next->size;
            current->next = next->next;
        } else {
            current = current->next;
        }
    }
}

// Frigör ett block
void mem_free(void* ptr) {
    if (!ptr) return;
    pthread_mutex_lock(&mem_lock);

    Block* block = (Block*)((char*)ptr - BLOCK_SIZE);
    if (block->free) {
        fprintf(stderr, "Varning: dubbel frigöring!\n");
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    block->free = 1;
    merge_free_blocks();

    pthread_mutex_unlock(&mem_lock);
}

// Ändra storleken på ett block (om nödvändigt)
void* mem_resize(void* ptr, size_t size) {
    if (!ptr) return mem_alloc(size);
    if (size == 0) {
        mem_free(ptr);
        return NULL;
    }

    pthread_mutex_lock(&mem_lock);
    Block* block = (Block*)((char*)ptr - BLOCK_SIZE);
    if (block->size >= size) {
        pthread_mutex_unlock(&mem_lock);
        return ptr;
    }
    pthread_mutex_unlock(&mem_lock);

    void* new_ptr = mem_alloc(size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, block->size);
    mem_free(ptr);
    return new_ptr;
}

// Rensar hela minnespoolen
void mem_deinit() {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        free(memory_pool);
        memory_pool = NULL;
        free_list = NULL;
        pool_size = 0;
    }
    pthread_mutex_unlock(&mem_lock);
    pthread_mutex_destroy(&mem_lock);
}
