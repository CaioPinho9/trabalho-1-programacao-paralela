#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

// node_queue_t structure for the linked list
typedef struct node_queue_t
{
    void *data;
    struct node_queue_t *next;
} node_queue_t;

// queue_t structure
typedef struct queue_t
{
    node_queue_t *front;
    node_queue_t *rear;
    sem_t sem;
    pthread_mutex_t mutex;
} queue_t;

// Create a new node
node_queue_t *create_node_queue(void *data)
{
    node_queue_t *new_node = (node_queue_t *)malloc(sizeof(node_queue_t));
    new_node->data = data;
    new_node->next = NULL;
    return new_node;
}

// Initialize a queue
queue_t *create_queue()
{
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
    queue->front = queue->rear = NULL;
    sem_init(&queue->sem, 0, 0);
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

// Enqueue (add to the rear of the queue)
void enqueue(queue_t *queue, void *data)
{
    node_queue_t *new_node = create_node_queue(data);

    pthread_mutex_lock(&queue->mutex);
    // If the queue is empty, both front and rear point to the new node
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = new_node;
        return;
    }

    // Add the new node at the end and change rear
    queue->rear->next = new_node;
    queue->rear = new_node;
    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->sem);
}

// Dequeue (remove from the front of the queue)
void *dequeue(queue_t *queue)
{
    pthread_mutex_lock(&queue->mutex);

    if (queue->front == NULL)
    {
        printf("Queue is empty! Cannot dequeue.\n");
        return NULL; // Indicates an error
    }

    // Store the previous front and move front to the next node
    node_queue_t *temp = queue->front;
    void *data = temp->data;
    queue->front = queue->front->next;

    // If front becomes NULL, then rear should also be NULL
    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }

    free(temp);
    pthread_mutex_unlock(&queue->mutex);
    return data;
}

// Peek (get the front element without removing it)
void *peek(queue_t *queue)
{
    pthread_mutex_lock(&queue->mutex);
    if (queue->front == NULL)
    {
        printf("Queue is empty!\n");
        return NULL;
    }
    void *data = queue->front->data;
    pthread_mutex_unlock(&queue->mutex);
    return data;
}

// Check if the queue is empty
int is_empty(queue_t *queue)
{
    return queue->front == NULL;
}
