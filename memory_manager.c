#define _POSIX_C_SOURCE 200809L
#include "memory_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

typedef struct block_header {
    size_t size;                // storlek på användbart block (exkl. header)
    int free;                   // 1 = ledigt, 0 = upptaget
    struct block_header* next;
} block_header_t;

static void* memory_pool = NULL;
static size_t pool_size = 0;
static block_header_t* first_block = NULL;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;
static const size_t ALIGN = sizeof(void*);

static size_t align_size(size_t s) {
    return (s + ALIGN - 1) & ~(ALIGN - 1);
}

void mem_init(size_t size) {
    if (size == 0) return;
    pthread_mutex_lock(&mem_lock);

    if (memory_pool) { pthread_mutex_unlock(&mem_lock); return; }

    memory_pool = malloc(size);  // exakt storlek för användarblock
    if (!memory_pool) {
        fprintf(stderr, "mem_init: malloc failed\n");
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    pool_size = size;
    first_block = (block_header_t*)memory_pool;
    first_block->size = size - sizeof(block_header_t);
    first_block->free = 1;
    first_block->next = NULL;

    pthread_mutex_unlock(&mem_lock);
}

static block_header_t* find_fit(block_header_t** prev, size_t size) {
    block_header_t* cur = first_block;
    *prev = NULL;
    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        *prev = cur;
        cur = cur->next;
    }
    return NULL;
}

void* mem_alloc(size_t size) {
    if (size == 0) return NULL;
    size = align_size(size);

    pthread_mutex_lock(&mem_lock);
    if (!memory_pool) { pthread_mutex_unlock(&mem_lock); return NULL; }

    block_header_t* prev = NULL;
    block_header_t* fit = find_fit(&prev, size);
    if (!fit) { pthread_mutex_unlock(&mem_lock); return NULL; }

    size_t remaining = fit->size - size;

    if (remaining >= sizeof(block_header_t) + ALIGN) {
        uint8_t* base = (uint8_t*)fit;
        block_header_t* new_block = (block_header_t*)(base + sizeof(block_header_t) + size);
        new_block->size = remaining - sizeof(block_header_t);
        new_block->free = 1;
        new_block->next = fit->next;
        fit->next = new_block;
        fit->size = size;
    }

    fit->free = 0;
    void* user_ptr = (uint8_t*)fit + sizeof(block_header_t);
    pthread_mutex_unlock(&mem_lock);
    return user_ptr;
}

static block_header_t* header_from_ptr(void* ptr) {
    return ptr ? (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t)) : NULL;
}

void mem_free(void* block) {
    if (!block) return;
    pthread_mutex_lock(&mem_lock);
    if (!memory_pool) { pthread_mutex_unlock(&mem_lock); return; }

    block_header_t* hdr = header_from_ptr(block);
    if (hdr->free) { pthread_mutex_unlock(&mem_lock); return; }
    hdr->free = 1;

    block_header_t* cur = first_block;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next = cur->next->next;
            continue;
        }
        cur = cur->next;
    }

    pthread_mutex_unlock(&mem_lock);
}

void* mem_resize(void* block, size_t size) {
    if (!block) return mem_alloc(size);
    if (size == 0) { mem_free(block); return NULL; }

    size = align_size(size);
    pthread_mutex_lock(&mem_lock);

    block_header_t* hdr = header_from_ptr(block);
    if (!hdr) { pthread_mutex_unlock(&mem_lock); return NULL; }

    if (hdr->size >= size) {
        size_t remaining = hdr->size - size;
        if (remaining >= sizeof(block_header_t) + ALIGN) {
            uint8_t* base = (uint8_t*)hdr;
            block_header_t* new_block = (block_header_t*)(base + sizeof(block_header_t) + size);
            new_block->size = remaining - sizeof(block_header_t);
            new_block->free = 1;
            new_block->next = hdr->next;
            hdr->next = new_block;
            hdr->size = size;
        }
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    if (hdr->next && hdr->next->free) {
        size_t combined = hdr->size + sizeof(block_header_t) + hdr->next->size;
        if (combined >= size) {
            block_header_t* next = hdr->next;
            hdr->size = combined;
            hdr->next = next->next;
            hdr->free = 0;

            size_t remaining = hdr->size - size;
            if (remaining >= sizeof(block_header_t) + ALIGN) {
                uint8_t* base = (uint8_t*)hdr;
                block_header_t* new_block = (block_header_t*)(base + sizeof(block_header_t) + size);
                new_block->size = remaining - sizeof(block_header_t);
                new_block->free = 1;
                new_block->next = hdr->next;
                hdr->next = new_block;
                hdr->size = size;
            }
            pthread_mutex_unlock(&mem_lock);
            return block;
        }
    }

    pthread_mutex_unlock(&mem_lock);

    void* new_blk = mem_alloc(size);
    if (!new_blk) return NULL;

    size_t to_copy = hdr->size < size ? hdr->size : size;
    memcpy(new_blk, block, to_copy);
    mem_free(block);
    return new_blk;
}

void mem_deinit() {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
        free(memory_pool);
        memory_pool = NULL;
        pool_size = 0;
        first_block = NULL;
    }
    pthread_mutex_unlock(&mem_lock);
}
