#include <stdio.h>
#include <stdlib.h>

// NodeQueue structure for the linked list
typedef struct NodeQueue
{
    void *data;
    struct NodeQueue *next;
} NodeQueue;

// Queue structure
typedef struct Queue
{
    NodeQueue *front;
    NodeQueue *rear;
} Queue;

// Create a new node
NodeQueue *create_node_queue(void *data)
{
    NodeQueue *new_node = (NodeQueue *)malloc(sizeof(NodeQueue));
    new_node->data = data;
    new_node->next = NULL;
    return new_node;
}

// Initialize a queue
Queue *create_queue()
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    queue->front = queue->rear = NULL;
    return queue;
}

// Enqueue (add to the rear of the queue)
void enqueue(Queue *queue, void *data)
{
    NodeQueue *new_node = create_node_queue(data);

    // If the queue is empty, both front and rear point to the new node
    if (queue->rear == NULL)
    {
        queue->front = queue->rear = new_node;
        return;
    }

    // Add the new node at the end and change rear
    queue->rear->next = new_node;
    queue->rear = new_node;
}

// Dequeue (remove from the front of the queue)
void *dequeue(Queue *queue)
{
    if (queue->front == NULL)
    {
        printf("Queue is empty! Cannot dequeue.\n");
        return NULL; // Indicates an error
    }

    // Store the previous front and move front to the next node
    NodeQueue *temp = queue->front;
    void *data = temp->data;
    queue->front = queue->front->next;

    // If front becomes NULL, then rear should also be NULL
    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }

    free(temp);
    return data;
}

// Peek (get the front element without removing it)
void *peek(Queue *queue)
{
    if (queue->front == NULL)
    {
        printf("Queue is empty!\n");
        return NULL;
    }
    return queue->front->data;
}

// Check if the queue is empty
int is_empty(Queue *queue)
{
    return queue->front == NULL;
}
