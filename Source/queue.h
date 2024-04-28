#ifndef QUEUE_H
#define QUEUE_H

#include "global.h"

typedef struct Node {
    char data[PIPE_BUFFER_SIZE]; // Changed from int to char[PIPE_BUFFER_SIZE
    struct Node* next;
} Node;

typedef struct Queue {
    int num_elements;
    int max_elements;
    Node* front;
    Node* rear;
} Queue;

Queue* create_queue(int max_elements);
int is_empty(Queue* queue);
int is_full(Queue* queue);
void push(Queue* queue, char *data);
char *pop(Queue* queue);
char *front(Queue* queue);
char *rear(Queue* queue);


#endif