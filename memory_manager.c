#include "memory_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Initiering - gör inget speciellt.
 */
void mem_init(size_t size) {
    // Ingen åtgärd behövs
}

/**
 * Allokera minne med malloc.
 */
void *mem_alloc(size_t size) {
    return malloc(size);
}

/**
 * Frigör minne med free.
 */
void mem_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

/**
 * Ändra storlek på minne med realloc.
 */
void *mem_resize(void *ptr, size_t size) {
    if (!ptr) {
        return mem_alloc(size);
    }
    return realloc(ptr, size);
}

/**
 * Deinitiering - gör inget speciellt.
 */
void mem_deinit(void) {
    // Ingen åtgärd behövs
}
