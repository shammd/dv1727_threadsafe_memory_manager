#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h>   // för size_t
#include <pthread.h>  // för trådsäkerhet

// Initierar minneshanteraren med en viss pool-storlek
void mem_init(size_t size);

// Allokerar ett block av angiven storlek från poolen
void* mem_alloc(size_t size);

// Frigör ett tidigare allokerat block
void mem_free(void* block);

// Ändrar storleken på ett block (flyttar det om det behövs)
void* mem_resize(void* block, size_t size);

// Rensar hela poolen och frigör allt minne
void mem_deinit(void);

#endif
