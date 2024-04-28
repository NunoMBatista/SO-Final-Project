#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "queue.h"

Queue* create_queue(int max_elements) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->num_elements = 0;
    queue->max_elements = max_elements;
    queue->front = queue->rear = NULL;
    return queue;
}

int is_empty(Queue* queue) {
    return queue->num_elements == 0;
}

int is_full(Queue *queue){
    return queue->num_elements == queue->max_elements;
}

void push(Queue* queue, char *data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    strncpy(newNode->data, data, PIPE_BUFFER_SIZE);
    newNode->next = NULL;

    if (queue->rear == NULL) {
        queue->front = queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    queue->num_elements++;
}

char* pop(Queue* queue) {
    if (is_empty(queue)) {
        return NULL;
    }

    Node* temp = queue->front;
    char* data = (char*) malloc(PIPE_BUFFER_SIZE * sizeof(char));

    strcpy(data, temp->data);

    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    queue->num_elements--;

    return data;
}

char* front(Queue* queue) {
    if (is_empty(queue)) {
        return NULL;
    }
    return queue->front->data;
}
