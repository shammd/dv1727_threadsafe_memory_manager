#include "memory_manager.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef _WIN32
    #include <sys/mman.h>   // mmap, munmap (Linux, CodeGrade)
#else
    #include <stdlib.h>     // malloc, free (Windows fallback)
#endif

#define MAX_BLOCKS 1024
static const size_t ALIGN = sizeof(void*);

typedef struct {
    size_t offset;  // offset i bytes från memory_pool
    size_t size;    // blockstorlek i bytes
    int free;       // 1 = ledigt, 0 = allokerat
} Block;

static void* memory_pool = NULL;
static size_t pool_size = 0;
static Block blocks[MAX_BLOCKS];
static size_t num_blocks = 0;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t align_size(size_t s) {
    if (s == 0) return 0;
    return (s + (ALIGN - 1)) & ~(ALIGN - 1);
}

/**
 * mem_init:
 * - På Linux: allokerar poolen med mmap(size)
 * - På Windows: använder malloc(size) bara för lokal testning
 */
void mem_init(size_t size) {
    if (size == 0) return;
    size = align_size(size);

    pthread_mutex_lock(&mem_lock);

    if (memory_pool != NULL) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

#ifndef _WIN32
    void* region = mmap(NULL, size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
    if (region == MAP_FAILED) {
        fprintf(stderr, "mem_init: mmap(%zu) failed\n", size);
        pthread_mutex_unlock(&mem_lock);
        return;
    }
#else
    void* region = malloc(size);
    if (!region) {
        fprintf(stderr, "mem_init: malloc(%zu) failed\n", size);
        pthread_mutex_unlock(&mem_lock);
        return;
    }
#endif

    memory_pool = region;
    pool_size = size;

    blocks[0].offset = 0;
    blocks[0].size   = size;
    blocks[0].free   = 1;
    num_blocks = 1;

    pthread_mutex_unlock(&mem_lock);
}

void* mem_alloc(size_t size) {
    if (size == 0) return NULL;
    size = align_size(size);

    pthread_mutex_lock(&mem_lock);
    if (memory_pool == NULL) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    for (size_t i = 0; i < num_blocks; ++i) {
        if (blocks[i].free && blocks[i].size >= size) {
            size_t offset = blocks[i].offset;

            if (blocks[i].size == size || num_blocks >= MAX_BLOCKS) {
                blocks[i].free = 0;
            } else {
                size_t old_size = blocks[i].size;
                size_t new_free_offset = offset + size;
                size_t new_free_size   = old_size - size;

                for (size_t j = num_blocks; j > i + 1; --j) {
                    blocks[j] = blocks[j - 1];
                }
                blocks[i].size = size;
                blocks[i].free = 0;

                blocks[i + 1].offset = new_free_offset;
                blocks[i + 1].size   = new_free_size;
                blocks[i + 1].free   = 1;
                num_blocks++;
            }

            void* ptr = (uint8_t*)memory_pool + offset;
            pthread_mutex_unlock(&mem_lock);
            return ptr;
        }
    }

    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

void mem_free(void* block) {
    if (!block) return;

    pthread_mutex_lock(&mem_lock);
    if (memory_pool == NULL) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    uintptr_t offset = (uintptr_t)((uint8_t*)block - (uint8_t*)memory_pool);

    size_t idx = num_blocks;
    for (size_t i = 0; i < num_blocks; ++i) {
        if (blocks[i].offset == offset) {
            idx = i;
            break;
        }
    }
    if (idx == num_blocks) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    if (blocks[idx].free) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    blocks[idx].free = 1;

    size_t i = 0;
    while (i + 1 < num_blocks) {
        if (blocks[i].free && blocks[i + 1].free &&
            blocks[i].offset + blocks[i].size == blocks[i + 1].offset) {

            blocks[i].size += blocks[i + 1].size;

            for (size_t j = i + 1; j + 1 < num_blocks; ++j) {
                blocks[j] = blocks[j + 1];
            }
            num_blocks--;
        } else {
            ++i;
        }
    }

    pthread_mutex_unlock(&mem_lock);
}

void* mem_resize(void* block, size_t size) {
    if (!block) {
        return mem_alloc(size);
    }
    if (size == 0) {
        mem_free(block);
        return NULL;
    }

    size = align_size(size);

    pthread_mutex_lock(&mem_lock);
    if (memory_pool == NULL) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    uintptr_t offset = (uintptr_t)((uint8_t*)block - (uint8_t*)memory_pool);

    size_t idx = num_blocks;
    for (size_t i = 0; i < num_blocks; ++i) {
        if (blocks[i].offset == offset) {
            idx = i;
            break;
        }
    }
    if (idx == num_blocks) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    size_t old_size = blocks[idx].size;

    if (size <= old_size) {
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    if (idx + 1 < num_blocks &&
        blocks[idx + 1].free &&
        blocks[idx].offset + blocks[idx].size == blocks[idx + 1].offset &&
        blocks[idx].size + blocks[idx + 1].size >= size) {

        size_t extra = size - blocks[idx].size;
        blocks[idx].size += extra;
        blocks[idx + 1].offset += extra;
        blocks[idx + 1].size   -= extra;

        if (blocks[idx + 1].size == 0) {
            for (size_t j = idx + 1; j + 1 < num_blocks; ++j) {
                blocks[j] = blocks[j + 1];
            }
            num_blocks--;
        }

        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    pthread_mutex_unlock(&mem_lock);

    void* new_ptr = mem_alloc(size);
    if (!new_ptr) {
        return NULL;
    }

    size_t to_copy = old_size < size ? old_size : size;
    memcpy(new_ptr, block, to_copy);
    mem_free(block);
    return new_ptr;
}

void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);

    if (memory_pool != NULL) {
#ifndef _WIN32
        munmap(memory_pool, pool_size);
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size = 0;
        num_blocks = 0;
    }

    pthread_mutex_unlock(&mem_lock);
}
