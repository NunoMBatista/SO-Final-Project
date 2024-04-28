#include <stdio.h>
#include "queue.h"

int main (void){
    Queue* queue = create_queue();
    printf("Queue is empty: %d\n", is_empty(queue));

    enqueue(queue, "Test1");
    enqueue(queue, "Test2");
    enqueue(queue, "Test3");
    enqueue(queue, "Test4");

    while(!isEmpty(queue)){
        printf("Front: %s\n", front(queue));
        printf("Dequeued: %s\n", pop(queue));
    }

    return 0;
}