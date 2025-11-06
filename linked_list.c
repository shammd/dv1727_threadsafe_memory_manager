#include "linked_list.h"
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t list_lock; //Lås för hela listan

// Initierar listan och minneshanteraren
void list_init(Node** head, size_t size) {
    *head = NULL;
    mem_init(size);
    pthread_mutex_init(&list_lock, NULL);
}

// Lägger till ny nod sist i listan
void list_insert(Node** head, uint16_t data) {
    pthread_mutex_lock(&list_lock);

    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf("Minnet fullt\n");
        pthread_mutex_unlock(&list_lock);
        return;
    }

    new_node->data = data;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
    } else {
        Node* temp = *head;
        while (temp->next != NULL)
            temp = temp->next;
        temp->next = new_node;
    }

    pthread_mutex_unlock(&list_lock);
}

// Lägger till en ny nod direkt efter en vald nod
void list_insert_after(Node* prev_node, uint16_t data) {
    if (prev_node == NULL) return;
    pthread_mutex_lock(&list_lock);

    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf("Minnet fullt\n");
        pthread_mutex_unlock(&list_lock);
        return;
    }

    new_node->data = data;
    new_node->next = prev_node->next;
    prev_node->next = new_node;

    pthread_mutex_unlock(&list_lock);
}

// Lägger till en nod före en viss nod
void list_insert_before(Node** head, Node* next_node, uint16_t data) {
    if (next_node == NULL || head == NULL) return;
    pthread_mutex_lock(&list_lock);

    Node* new_node = (Node*)mem_alloc(sizeof(Node));
    if (!new_node) {
        printf("Minnet fullt\n");
        pthread_mutex_unlock(&list_lock);
        return;
    }

    new_node->data = data;

    // Om det är 1a noden
    if (*head == next_node) {
        new_node->next = *head;
        *head = new_node;
    } else {
        Node* prev = *head;
        while (prev && prev->next != next_node)
            prev = prev->next;
        if (prev) {
            new_node->next = next_node;
            prev->next = new_node;
        } else {
            printf("Noden hittades inte\n");
            mem_free(new_node);
        }
    }

    pthread_mutex_unlock(&list_lock);
}

// tar bort en nod med visst värde
void list_delete(Node** head, uint16_t data) {
    pthread_mutex_lock(&list_lock);

    if (*head == NULL) {
        pthread_mutex_unlock(&list_lock);
        return;
    }

    Node* temp = *head;
    Node* prev = NULL;

    while (temp && temp->data != data) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        pthread_mutex_unlock(&list_lock);
        return;
    }

    if (prev == NULL)
        *head = temp->next;
    else
        prev->next = temp->next;

    mem_free(temp);
    pthread_mutex_unlock(&list_lock);
}

// Söker efter en nod med visst värde
Node* list_search(Node** head, uint16_t data) {
    pthread_mutex_lock(&list_lock);
    Node* current = *head;

    while (current) {
        if (current->data == data) {
            pthread_mutex_unlock(&list_lock);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list_lock);
    return NULL;
}

// Skriver ut hela listan
void list_display(Node** head) {
    pthread_mutex_lock(&list_lock);
    Node* temp = *head;

    printf("[");
    while (temp) {
        printf("%u", temp->data);
        if (temp->next) printf(", ");
        temp = temp->next;
    }
    printf("]\n");

    pthread_mutex_unlock(&list_lock);
}

// Skriver ut noder mellan de två givna noder
void list_display_range(Node** head, Node* start_node, Node* end_node) {
    pthread_mutex_lock(&list_lock);
    Node* temp = *head;

    if (start_node == NULL)
        start_node = *head;

    int printing = 0;
    printf("[");

    while (temp) {
        if (temp == start_node) printing = 1;
        if (printing) {
            printf("%u", temp->data);
            if (temp != end_node && temp->next != NULL) printf(", ");
        }
        if (temp == end_node) break;
        temp = temp->next;
    }

    printf("]\n");
    pthread_mutex_unlock(&list_lock);
}

// Räknar antalet noder i listan
int list_count_nodes(Node** head) {
    pthread_mutex_lock(&list_lock);
    int count = 0;
    Node* temp = *head;

    while (temp) {
        count++;
        temp = temp->next;
    }

    pthread_mutex_unlock(&list_lock);
    return count;
}

// Frigör alla noder och nollställer listan
void list_cleanup(Node** head) {
    pthread_mutex_lock(&list_lock);

    Node* current = *head;
    Node* next;
    while (current) {
        next = current->next;
        mem_free(current);
        current = next;
    }

    *head = NULL;
    mem_deinit();

    pthread_mutex_unlock(&list_lock);
    pthread_mutex_destroy(&list_lock);
}
