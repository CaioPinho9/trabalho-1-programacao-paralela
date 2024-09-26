#ifdef QUEUE_H
#define QUEUE_H

typedef struct Queue
{
    NodeQueue *front;
    NodeQueue *rear;
} Queue;

NodeQueue *create_node(void *data);
Queue *create_queue();
void enqueue(Queue *queue, void *data);
void *dequeue(Queue *queue);
void *peek(Queue *queue);
int is_empty(Queue *queue);

#endif // QUEUE_H