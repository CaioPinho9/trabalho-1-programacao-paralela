#ifndef QUEUE_H
#define QUEUE_H

#include <semaphore.h>
#include <pthread.h>

typedef struct NodeQueue
{
    void *data;
    struct NodeQueue *next;
} NodeQueue;

typedef struct Queue
{
    NodeQueue *front;
    NodeQueue *rear;
    sem_t sem;
    pthread_mutex_t mutex;
} Queue;

NodeQueue *create_node(void *data);
Queue *create_queue();
void enqueue(Queue *queue, void *data);
void *dequeue(Queue *queue);
void *peek(Queue *queue);
int is_empty(Queue *queue);

#endif // QUEUE_H