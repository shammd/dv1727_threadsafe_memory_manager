#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdint.h>   // för uint16_t
#include <stddef.h>   // för size_t
#include <pthread.h>  // för trådsäkerhet
#include "memory_manager.h"

// Nodstruktur för den länkade listan
typedef struct Node {
    uint16_t data;      // värdet i noden
    struct Node* next;  // pekare till nästa nod
} Node;

// Initierar listan och minneshanteraren
void list_init(Node** head, size_t size);

// Lägger till en ny nod sist i listan
void list_insert(Node** head, uint16_t data);

// Lägger till en ny nod efter en vald nod
void list_insert_after(Node* prev_node, uint16_t data);

// Lägger till en ny nod före en vald nod
void list_insert_before(Node** head, Node* next_node, uint16_t data);

// Tar bort en nod med visst värde
void list_delete(Node** head, uint16_t data);

// Söker efter en nod med ett visst värde
Node* list_search(Node** head, uint16_t data);

// Skriver ut hela listan
void list_display(Node** head);

// Skriver ut noder mellan två givna noder
void list_display_range(Node** head, Node* start_node, Node* end_node);

// Räknar antalet noder i listan
int list_count_nodes(Node** head);

// Frigör alla noder och rensar listan
void list_cleanup(Node** head);

#endif
