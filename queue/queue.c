#include <stdio.h>
#include <stdlib.h>

// Node structure for the linked list
typedef struct Node {
    void* data;
    struct Node* next;
} Node;

// Queue structure
typedef struct Queue {
    Node* front;
    Node* rear;
} Queue;

// Create a new node
Node* create_node(void* data) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = NULL;
    return new_node;
}

// Initialize a queue
Queue* create_queue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->front = queue->rear = NULL;
    return queue;
}

// Enqueue (add to the rear of the queue)
void enqueue(Queue* queue, void* data) {
    Node* new_node = create_node(data);

    // If the queue is empty, both front and rear point to the new node
    if (queue->rear == NULL) {
        queue->front = queue->rear = new_node;
        return;
    }

    // Add the new node at the end and change rear
    queue->rear->next = new_node;
    queue->rear = new_node;
}

// Dequeue (remove from the front of the queue)
void* dequeue(Queue* queue) {
    if (queue->front == NULL) {
        printf("Queue is empty! Cannot dequeue.\n");
        return -1; // Indicates an error
    }

    // Store the previous front and move front to the next node
    Node* temp = queue->front;
    void* data = temp->data;
    queue->front = queue->front->next;

    // If front becomes NULL, then rear should also be NULL
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    return data;
}

// Peek (get the front element without removing it)
void* peek(Queue* queue) {
    if (queue->front == NULL) {
        printf("Queue is empty!\n");
        return -1;
    }
    return queue->front->data;
}

// Check if the queue is empty
int is_empty(Queue* queue) {
    return queue->front == NULL;
}

// Display the queue elements
void display_queue(Queue* queue) {
    if (queue->front == NULL) {
        printf("Queue is empty.\n");
        return;
    }

    Node* temp = queue->front;
    while (temp != NULL) {
        printf("%d -> ", temp->data);
        temp = temp->next;
    }
    printf("NULL\n");
}