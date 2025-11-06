#include "linked_list.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // för sleep/usleep

// Global lista som alla trådar delar
Node* head = NULL;

// Funktion som körs i varje tråd
void* thread_task(void* arg) {
    int thread_id = *((int*)arg);

    // Varje tråd lägger in 5 värden i listan
    for (int i = 0; i < 5; i++) {
        uint16_t value = (thread_id * 10) + i;
        list_insert(&head, value);
        printf("Thread %d inserted %u\n", thread_id, value);
        usleep(100000); // liten paus (0.1 sek) för att visa parallell körning
    }

    return NULL;
}

int main() {
    printf("Thread-Safe Linked List Test \n");

    // Startar listan med 2 KB minnespool
    list_init(&head, 2048);

    // Skapar 3 trådar som jobbar samtidigt
    pthread_t threads[3];
    int ids[3] = {1, 2, 3};

    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, thread_task, &ids[i]);
    }

    // Väntar på att alla trådar ska bli klara
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nFinal list contents:\n");
    list_display(&head);

    printf("Total nodes: %d\n", list_count_nodes(&head));

    // Testar sökning
    Node* found = list_search(&head, 12);
    if (found)
        printf("Found node with value 12 ✅\n");
    else
        printf("Value 12 not found ❌\n");

    // Testar borttagning
    list_delete(&head, 11);
    printf("After deleting 11:\n");
    list_display(&head);

    // Testar att visa ett intervall
    Node* start = list_search(&head, 10);
    Node* end = list_search(&head, 14);
    printf("Range [10,14]:\n");
    list_display_range(&head, start, end);

    // Städar upp allting
    list_cleanup(&head);
    printf("\nCleanup completed ✅\n");

    return 0;
}
