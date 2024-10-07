#ifndef QUEUE_H
#define QUEUE_H

#include <semaphore.h>
#include <pthread.h>

typedef struct node_queue_t
{
    void *data;
    struct node_queue_t *next;
} node_queue_t;

typedef struct queue_t
{
    node_queue_t *front;
    node_queue_t *rear;
    sem_t sem;
    pthread_mutex_t mutex;
} queue_t;

queue_t *create_queue();
void enqueue(queue_t *queue, void *data);
void *dequeue(queue_t *queue);

#endif // QUEUE_H