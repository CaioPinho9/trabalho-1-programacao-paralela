#include <stdio.h>
#include <stdlib.h>
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

node_queue_t *create_node_queue(void *data)
{
    node_queue_t *new_node = (node_queue_t *)malloc(sizeof(node_queue_t));
    new_node->data = data;
    new_node->next = NULL;
    return new_node;
}

queue_t *create_queue()
{
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    queue->front = queue->rear = NULL;
    sem_init(&queue->sem, 0, 0);
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

void enqueue(queue_t *queue, void *data)
{
    node_queue_t *new_node = create_node_queue(data);

    pthread_mutex_lock(&queue->mutex);
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = new_node;
    }
    else
    {
        queue->rear->next = new_node;
        queue->rear = new_node;
    }

    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->sem);
}

void *dequeue(queue_t *queue)
{
    pthread_mutex_lock(&queue->mutex);

    if (queue->front == NULL)
    {
        printf("Queue is empty! Cannot dequeue.\n");
        return NULL;
    }

    node_queue_t *temp = queue->front;
    void *data = temp->data;
    queue->front = queue->front->next;

    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }

    free(temp);
    pthread_mutex_unlock(&queue->mutex);
    return data;
}
