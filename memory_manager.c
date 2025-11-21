#include "memory_manager.h"

#include <stddef.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
  #include <windows.h>
  #include <stdlib.h>
  #define USE_MMAP 0
#else
  #include <sys/mman.h>
  #include <unistd.h>
  #include <stdlib.h>
  #define USE_MMAP 1
#endif

// ---------------------------
// Interna datastrukturer
// ---------------------------

typedef struct segment {
    size_t offset;              // offset från poolens början
    size_t size;                // segmentets storlek i bytes
    int    free;                // 1 = ledig, 0 = allokerad
    struct segment *next;       // nästa segment i adressordning
} segment_t;

// Vi lägger INTE metadata i poolen. Hela poolen är "user memory".
// Tester som analyserar blockens gränser ser då en ren, tät packning.

#define MAX_SEGMENTS 65536

static segment_t segments[MAX_SEGMENTS];
static segment_t *segment_head      = NULL; // första segmentet (beskriver poolen)
static segment_t *segment_free_list = NULL; // fria metadata-noder

static void  *memory_pool = NULL;
static size_t pool_size   = 0;

static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;

// ---------------------------
// Hjälpfunktioner
// ---------------------------

// Initiera freelist för metadata-noder
static void reset_segments(void) {
    segment_free_list = &segments[0];
    for (int i = 0; i < MAX_SEGMENTS - 1; ++i) {
        segments[i].next   = &segments[i + 1];
        segments[i].free   = 0;
        segments[i].size   = 0;
        segments[i].offset = 0;
    }
    segments[MAX_SEGMENTS - 1].next   = NULL;
    segments[MAX_SEGMENTS - 1].free   = 0;
    segments[MAX_SEGMENTS - 1].size   = 0;
    segments[MAX_SEGMENTS - 1].offset = 0;

    segment_head = NULL;
}

static segment_t *acquire_segment(void) {
    if (!segment_free_list) {
        return NULL; // slut på metadata-noder (borde aldrig hända i testet)
    }
    segment_t *s = segment_free_list;
    segment_free_list = s->next;
    s->next   = NULL;
    s->offset = 0;
    s->size   = 0;
    s->free   = 0;
    return s;
}

static void release_segment(segment_t *s) {
    if (!s) return;
    s->next = segment_free_list;
    segment_free_list = s;
}

// Hitta segmentet som börjar vid en viss offset. Returnera ev. föregående.
static segment_t *find_segment_by_offset(size_t offset, segment_t **prev_out) {
    segment_t *prev = NULL;
    segment_t *cur  = segment_head;
    while (cur) {
        if (cur->offset == offset) {
            if (prev_out) *prev_out = prev;
            return cur;
        }
        prev = cur;
        cur  = cur->next;
    }
    return NULL;
}

// Slå ihop angränsande fria segment -> mindre fragmentering.
static void coalesce_free_segments(void) {
    segment_t *cur = segment_head;
    while (cur && cur->next) {
        segment_t *next = cur->next;
        if (cur->free && next->free &&
            cur->offset + cur->size == next->offset) {
            // merg:a next in i cur
            cur->size += next->size;
            cur->next  = next->next;
            release_segment(next);
            // stanna kvar på cur – kanske finns fler fria efter
        } else {
            cur = cur->next;
        }
    }
}

// ---------------------------
// Publik API
// ---------------------------

void mem_init(size_t size) {
    if (size == 0) {
        return;
    }

    pthread_mutex_lock(&mem_lock);

    // Om mem_init anropas igen utan mem_deinit – städa gammalt.
    if (memory_pool != NULL) {
#if USE_MMAP
        munmap(memory_pool, pool_size);
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size   = 0;
    }

#if USE_MMAP
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }
    memory_pool = ptr;
#else
    memory_pool = malloc(size);
    if (!memory_pool) {
        pthread_mutex_unlock(&mem_lock);
        return;
    }
#endif

    pool_size = size;

    // Initiera metadata och skapa ett enda stort fritt segment
    reset_segments();
    segment_t *first = acquire_segment();
    if (!first) {
        // orimligt, men clean up ifall
#if USE_MMAP
        munmap(memory_pool, pool_size);
#else
        free(memory_pool);
#endif
        memory_pool = NULL;
        pool_size   = 0;
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    first->offset = 0;
    first->size   = pool_size;
    first->free   = 1;
    first->next   = NULL;
    segment_head  = first;

    pthread_mutex_unlock(&mem_lock);
}

void *mem_alloc(size_t size) {
    if (memory_pool == NULL) {
        // mem_init har inte körts
        return NULL;
    }

    // Tester vill att mem_alloc(0) ska lyckas (block1 != NULL),
    // så vi behandlar 0 som en 1-byte-allokering.
    if (size == 0) {
        size = 1;
    }

    pthread_mutex_lock(&mem_lock);

    segment_t *cur = segment_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            // Hittade ett fritt segment stort nog
            if (cur->size == size) {
                cur->free = 0;
                void *ptr = (char *)memory_pool + cur->offset;
                pthread_mutex_unlock(&mem_lock);
                return ptr;
            } else {
                // Splitta: cur blir allokerad, rest blir nytt fritt segment
                if (!segment_free_list) {
                    pthread_mutex_unlock(&mem_lock);
                    return NULL;
                }

                segment_t *rest = acquire_segment();
                rest->offset = cur->offset + size;
                rest->size   = cur->size - size;
                rest->free   = 1;
                rest->next   = cur->next;

                cur->size = size;
                cur->free = 0;
                cur->next = rest;

                void *ptr = (char *)memory_pool + cur->offset;
                pthread_mutex_unlock(&mem_lock);
                return ptr;
            }
        }
        cur = cur->next;
    }

    // Ingen plats kvar
    pthread_mutex_unlock(&mem_lock);
    return NULL;
}

void mem_free(void *ptr) {
    if (!ptr || !memory_pool) {
        return;
    }

    pthread_mutex_lock(&mem_lock);

    size_t offset = (size_t)((char *)ptr - (char *)memory_pool);
    if (offset >= pool_size) {
        // Pekaren ligger utanför poolen – ignorera
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    segment_t *prev = NULL;
    segment_t *seg  = find_segment_by_offset(offset, &prev);
    if (!seg) {
        // Okänd pekare – ignorera
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    seg->free = 1;
    coalesce_free_segments();

    pthread_mutex_unlock(&mem_lock);
}

void *mem_resize(void *ptr, size_t size) {
    if (ptr == NULL) {
        // Som malloc
        return mem_alloc(size);
    }

    if (!memory_pool) {
        return NULL;
    }

    // Behandla resize till 0 som resize till 1 byte + free gamla
    if (size == 0) {
        void *new_ptr = mem_alloc(1);
        if (new_ptr) {
            mem_free(ptr);
        }
        return new_ptr;
    }

    pthread_mutex_lock(&mem_lock);

    size_t offset = (size_t)((char *)ptr - (char *)memory_pool);
    if (offset >= pool_size) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    segment_t *prev = NULL;
    segment_t *seg  = find_segment_by_offset(offset, &prev);
    if (!seg) {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    size_t old_size = seg->size;

    // 1) Krympa eller samma storlek → stanna på samma ställe
    if (size <= old_size) {
        size_t remaining = old_size - size;
        if (remaining > 0) {
            if (!segment_free_list) {
                // inga metadata-noder kvar, men blocket är ändå giltigt
                pthread_mutex_unlock(&mem_lock);
                return ptr;
            }
            // Skapa nytt fritt segment efter det här
            segment_t *rest = acquire_segment();
            rest->offset = seg->offset + size;
            rest->size   = remaining;
            rest->free   = 1;
            rest->next   = seg->next;

            seg->size = size;
            seg->next = rest;

            coalesce_free_segments();
        }
        pthread_mutex_unlock(&mem_lock);
        return ptr;
    }

    // 2) Försök växa in i nästa fria, angränsande segment
    segment_t *next = seg->next;
    if (next && next->free &&
        seg->offset + seg->size == next->offset &&
        seg->size + next->size >= size) {

        size_t total = seg->size + next->size;

        if (total == size) {
            // Ät upp next helt
            seg->size = size;
            seg->next = next->next;
            release_segment(next);
        } else {
            // Krymp nästa fria
            next->offset = seg->offset + size;
            next->size   = total - size;
            seg->size    = size;
        }

        pthread_mutex_unlock(&mem_lock);
        return ptr;
    }

    // 3) Går inte att växa på plats → allokera ny, kopiera, fria gammal
    pthread_mutex_unlock(&mem_lock);

    void *new_ptr = mem_alloc(size);
    if (!new_ptr) {
        return NULL;
    }

    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    mem_free(ptr);
    return new_ptr;
}

void mem_deinit(void) {
    pthread_mutex_lock(&mem_lock);

    if (memory_pool) {
#if USE_MMAP
        munmap(memory_pool, pool_size);
#else
        free(memory_pool);
#endif
        memory_pool     = NULL;
        pool_size       = 0;
        segment_head    = NULL;
        segment_free_list = NULL;
    }

    pthread_mutex_unlock(&mem_lock);
}
