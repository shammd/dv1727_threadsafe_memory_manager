// memory_manager.c
#include "memory_manager.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

/* ============================
   Enkel first-fit allokator över en statisk arena.
   - Ingen malloc/mmap används.
   - Blocklayout: [Header][payload]
   - Header är inuti arenan, så vi kan traversera och koalescera.
   ============================ */

#define MAX_POOL_SIZE   (1u << 20)  /* 1 MiB arena som övre gräns – vi använder bara de första `pool_size` byten */
#define ALIGNMENT       8u

typedef struct BlockHeader {
    size_t size;                 /* storlek på payload i byte (exkl. header) */
    uint8_t free;                /* 1 = fri, 0 = allokerad */
    struct BlockHeader* next;    /* nästa block i listan */
} BlockHeader;

/* ---- Global arena + metadata ---- */
static uint8_t       g_pool[MAX_POOL_SIZE];   /* statiskt allokerad – inga systemanrop */
static size_t        g_pool_size = 0;         /* hur mycket av arenan som mem_init() aktiverade */
static BlockHeader*  g_head = NULL;           /* första blocket */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- Hjälpfunktioner ---- */
static inline size_t align_up(size_t n) {
    size_t r = (n + (ALIGNMENT - 1u)) & ~(ALIGNMENT - 1u);
    return r;
}

static inline size_t header_size(void) {
    return align_up(sizeof(BlockHeader));
}

static void split_block(BlockHeader* blk, size_t need) {
    /* Förutsätter: blk->free == 1, blk->size >= need */
    size_t hsz = header_size();
    if (blk->size >= need + hsz + ALIGNMENT) {
        /* Skapa ett nytt block efter payloaden */
        uint8_t* base = (uint8_t*)blk;
        BlockHeader* newblk = (BlockHeader*)(base + hsz + need);
        newblk->size = blk->size - need - hsz;
        newblk->free = 1;
        newblk->next = blk->next;

        blk->size = need;
        blk->next = newblk;
    }
    /* annars låter vi blocket vara “tight” utan split */
}

static void try_coalesce(BlockHeader* blk) {
    while (blk && blk->next && blk->free && blk->next->free) {
        BlockHeader* nxt = blk->next;
        blk->size += header_size() + nxt->size;
        blk->next = nxt->next;
    }
}

/* ---- Publika funktioner ---- */

void mem_init(size_t size) {
    pthread_mutex_lock(&g_lock);

    if (g_head != NULL) {
        /* redan initierad – idempotent */
        pthread_mutex_unlock(&g_lock);
        return;
    }

    if (size == 0 || size > MAX_POOL_SIZE) {
        /* klipp till giltig storlek */
        if (size == 0) size = ALIGNMENT;      /* minimal */
        if (size > MAX_POOL_SIZE) size = MAX_POOL_SIZE;
    }

    g_pool_size = align_up(size);

    /* Skapa ett stort fritt block över hela aktiva arenan */
    g_head = (BlockHeader*)g_pool;
    g_head->size = g_pool_size - header_size();
    g_head->free = 1u;
    g_head->next = NULL;

    pthread_mutex_unlock(&g_lock);
}

void* mem_alloc(size_t size) {
    if (size == 0) return NULL;

    pthread_mutex_lock(&g_lock);

    size_t need = align_up(size);
    BlockHeader* cur = g_head;

    while (cur) {
        if (cur->free && cur->size >= need) {
            split_block(cur, need);
            cur->free = 0u;
            void* payload = (uint8_t*)cur + header_size();
            pthread_mutex_unlock(&g_lock);
            return payload;
        }
        cur = cur->next;
    }

    pthread_mutex_unlock(&g_lock);
    return NULL; /* inget block stort nog */
}

void mem_free(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&g_lock);

    /* Header ligger precis före payloaden */
    BlockHeader* blk = (BlockHeader*)((uint8_t*)ptr - header_size());

    /* Grundläggande sanity: blocket måste ligga inom vår aktiva arena */
    uint8_t* pool_begin = g_pool;
    uint8_t* pool_end   = g_pool + g_pool_size;
    if ((uint8_t*)blk < pool_begin || (uint8_t*)blk >= pool_end) {
        /* utanför arenan – ignorera tyst eller logga vid behov */
        pthread_mutex_unlock(&g_lock);
        return;
    }

    blk->free = 1u;

    /* Koalescera: hitta föregångare för att kunna slå ihop i båda riktningar */
    BlockHeader* cur = g_head;
    BlockHeader* prev = NULL;

    while (cur) {
        if (cur == blk) {
            /* försök slå ihop höger */
            try_coalesce(cur);
            /* försök slå ihop vänster (om prev är fri) */
            if (prev && prev->free) {
                try_coalesce(prev); /* prev och cur slås ihop inuti */
            }
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    pthread_mutex_unlock(&g_lock);
}

void* mem_resize(void* ptr, size_t new_size) {
    if (ptr == NULL) {
        return mem_alloc(new_size);
    }
    if (new_size == 0) {
        mem_free(ptr);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);

    size_t need = align_up(new_size);
    BlockHeader* blk = (BlockHeader*)((uint8_t*)ptr - header_size());

    /* Om blocket redan är stort nog – behåll (ev. split för att minska slöseri) */
    if (blk->size >= need) {
        split_block(blk, need);
        pthread_mutex_unlock(&g_lock);
        return ptr;
    }

    /* Försök expandera in-place om nästa är fritt och tillsammans räcker */
    if (blk->next && blk->next->free) {
        size_t combined = blk->size + header_size() + blk->next->size;
        if (combined >= need) {
            /* “Låna” från nästa block */
            BlockHeader* nxt = blk->next;
            blk->size = combined;
            blk->next = nxt->next;
            /* Efter utökning, ev. split för att lämna rest som fritt block */
            split_block(blk, need);
            pthread_mutex_unlock(&g_lock);
            return (uint8_t*)blk + header_size();
        }
    }

    /* Annars: allokera nytt block, kopiera, fria gamla */
    pthread_mutex_unlock(&g_lock);

    void* new_ptr = mem_alloc(need);
    if (!new_ptr) {
        return NULL;
    }

    /* Kopiera min(old_size, need). Vi känner inte old_size externt, så läs header igen. */
    pthread_mutex_lock(&g_lock);
    size_t old_size = blk->size;
    pthread_mutex_unlock(&g_lock);

    size_t to_copy = old_size < need ? old_size : need;
    memcpy(new_ptr, ptr, to_copy);
    mem_free(ptr);
    return new_ptr;
}

void mem_deinit(void) {
    pthread_mutex_lock(&g_lock);
    /* Nollställ metadata – arenan är statisk, så inget att “free:a” */
    g_head = NULL;
    g_pool_size = 0;
    pthread_mutex_unlock(&g_lock);
}
