#ifdef QUEUE_H
#define QUEUE_H

typedef struct Queue {
    Node* front;
    Node* rear;
} Queue;

Node* create_node(void* data);
Queue* create_queue();
void enqueue(Queue* queue, void* data);
void* dequeue(Queue* queue);
void* peek(Queue* queue);
int is_empty(Queue* queue);
void display_queue(Queue* queue);

#endif // QUEUE_H