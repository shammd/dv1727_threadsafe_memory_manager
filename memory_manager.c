#define _POSIX_C_SOURCE 200809L

#include "memory_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

// Segment beskriver en intervall i poolen: [start, start+size)
typedef struct segment {
    size_t start;              // offset från baspekaren (i bytes)
    size_t size;               // längd (i bytes)
    int    free;               // 1 = fri, 0 = allokerad
    struct segment* next;
} segment_t;

static void*      memory_pool = NULL;
static size_t     pool_size   = 0;
static segment_t* seg_head    = NULL;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

#ifdef __linux__
static int used_mmap = 0;      // 1 om vi använde mmap, annars malloc
#else
static int used_mmap = 0;
#endif

// Särskild sentinel för size==0
static char  zero_sentinel;
static void* zero_ptr = &zero_sentinel;

// Hjälpfunktion: frigör all segment-metadata (inte poolen)
static void free_segments(void) {
    segment_t* cur = seg_head;
    while (cur) {
        segment_t* nxt = cur->next;
        free(cur);
        cur = nxt;
    }
    seg_head = NULL;
}

void mem_init(size_t size) {
    pthread_mutex_lock(&mem_lock);

    // Städa ev. gammal pool
    if (memory_pool) {
#ifdef __linux__
        if (used_mmap) {
            munmap(memory_pool, pool_size);
        } else {
            free(memory_pool);
        }
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size   = 0;
    }
    free_segments();

    if (size == 0) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    // Allokera poolen från OS (mmap på Linux, malloc fallback)
#ifdef __linux__
    void* p = mmap(NULL, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    if (p == MAP_FAILED) {
        p = malloc(size);
        used_mmap = 0;
    } else {
        used_mmap = 1;
    }
#else
    void* p = malloc(size);
    used_mmap = 0;
#endif

    if (!p) {
        perror("mem_init: allocation failed");
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }

    memory_pool = p;
    pool_size   = size;

    // Start med ett stort fritt segment: [0, size)
    segment_t* seg = (segment_t*)malloc(sizeof(segment_t));
    if (!seg) {
        perror("mem_init: segment malloc failed");
#ifdef __linux__
        if (used_mmap) munmap(memory_pool, pool_size);
        else free(memory_pool);
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size = 0;
        pthread_mutex_unlock(&mem_lock);
        exit(EXIT_FAILURE);
    }
    seg->start = 0;
    seg->size  = size;
    seg->free  = 1;
    seg->next  = NULL;
    seg_head   = seg;

    pthread_mutex_unlock(&mem_lock);
}

void* mem_alloc(size_t size) {
    // Testerna vill att mem_alloc(0) returnerar
    // *samma icke-NULL pekare* varje gång.
    if (size == 0) {
        return zero_ptr;
    }
    if (!memory_pool || pool_size == 0) {
        return NULL;
    }

    pthread_mutex_lock(&mem_lock);

    segment_t* cur = seg_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            size_t alloc_start = cur->start;

            if (cur->size == size) {
                // Hela segmentet går till användaren
                cur->free = 0;
            } else {
                // Splitta segmentet: [start, start+size) = alloc
                // och [start+size, ...] = ny fri del
                segment_t* new_free = (segment_t*)malloc(sizeof(segment_t));
                if (!new_free) {
                    pthread_mutex_unlock(&mem_lock);
                    return NULL;
                }
                new_free->start = cur->start + size;
                new_free->size  = cur->size - size;
                new_free->free  = 1;
                new_free->next  = cur->next;

                cur->size = size;
                cur->free = 0;
                cur->next = new_free;
            }

            uint8_t* base = (uint8_t*)memory_pool;
            void* ptr = base + alloc_start;   // OBS: ingen metadata inne i poolen
            pthread_mutex_unlock(&mem_lock);
            return ptr;
        }
        cur = cur->next;
    }

    // Ingen plats
    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

void mem_free(void* ptr) {
    if (!ptr) return;
    if (ptr == zero_ptr) return;
    if (!memory_pool) return;

    pthread_mutex_lock(&mem_lock);

    uint8_t* base = (uint8_t*)memory_pool;
    size_t off = (size_t)((uint8_t*)ptr - base);
    if (off >= pool_size) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    segment_t* prev = NULL;
    segment_t* cur  = seg_head;
    while (cur) {
        if (!cur->free && cur->start == off) {
            cur->free = 1;
            break;
        }
        prev = cur;
        cur  = cur->next;
    }

    if (!cur) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    // Merge med nästa om fri och direkt efter
    if (cur->next && cur->next->free &&
        cur->start + cur->size == cur->next->start) {
        segment_t* victim = cur->next;
        cur->size += victim->size;
        cur->next = victim->next;
        free(victim);
    }

    // Merge med föregående om fri och direkt före
    if (prev && prev->free &&
        prev->start + prev->size == cur->start) {
        prev->size += cur->size;
        prev->next = cur->next;
        free(cur);
    }

    pthread_mutex_unlock(&mem_lock);
}

void* mem_resize(void* block, size_t size) {
    if (block == NULL) {
        return mem_alloc(size);
    }
    if (size == 0) {
        mem_free(block);
        return zero_ptr;
    }
    if (!memory_pool) return NULL;

    pthread_mutex_lock(&mem_lock);

    uint8_t* base = (uint8_t*)memory_pool;
    size_t off = (size_t)((uint8_t*)block - base);
    segment_t* cur = seg_head;
    segment_t* prev = NULL;
    while (cur) {
        if (!cur->free && cur->start == off) break;
        prev = cur;
        cur  = cur->next;
    }

    if (!cur) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    size_t old_size = cur->size;

    // Om nya storleken är mindre: shrink in-place
    if (size <= old_size) {
        size_t shrink = old_size - size;
        if (shrink > 0) {
            cur->size = size;
            segment_t* new_free = (segment_t*)malloc(sizeof(segment_t));
            if (!new_free) {
                // Kunde inte splitta, behåll gammal storlek
                cur->size = old_size;
            } else {
                new_free->start = cur->start + size;
                new_free->size  = shrink;
                new_free->free  = 1;
                new_free->next  = cur->next;
                cur->next       = new_free;
            }
        }
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    // Försök växa in i nästa segment om det är fritt + direkt angränsande
    if (cur->next && cur->next->free &&
        cur->start + cur->size == cur->next->start &&
        cur->size + cur->next->size >= size) {

        size_t needed_extra = size - cur->size;

        if (needed_extra >= cur->next->size) {
            segment_t* victim = cur->next;
            cur->size += victim->size;
            cur->next = victim->next;
            free(victim);
        } else {
            cur->size += needed_extra;
            cur->next->start += needed_extra;
            cur->next->size  -= needed_extra;
        }
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    // Annars: lås upp och gör "alloc+copy+free" som fallback
    pthread_mutex_unlock(&mem_lock);

    void* new_block = mem_alloc(size);
    if (!new_block) return NULL;

    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_block, block, copy_size);
    mem_free(block);
    return new_block;
}

void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);
    if (memory_pool) {
#ifdef __linux__
        if (used_mmap) {
            munmap(memory_pool, pool_size);
        } else {
            free(memory_pool);
        }
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size   = 0;
    }
    free_segments();
    pthread_mutex_unlock(&mem_lock);
}
