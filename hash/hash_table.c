#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 100

// Node structure for chaining
typedef struct Node
{
    int id;
    int value;
    struct Node *next;
} Node;

// Hash table structure
typedef struct
{
    Node *buckets[TABLE_SIZE];
} HashTable;

// Simple hash function
unsigned int hash(int id)
{
    unsigned int hash = 0;
    while (id)
    {
        hash = (hash << 5) + id++;
    }
    return hash % TABLE_SIZE;
}

// Create a new node
Node *create_node(int id, int value)
{
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->id = id;
    new_node->value = value;
    new_node->next = NULL;
    return new_node;
}

// Insert into hash table
void insert(HashTable *table, int id, int value)
{
    unsigned int index = hash(id);
    Node *new_node = create_node(id, value);
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

// Search in hash table
int search(HashTable *table, int id)
{
    unsigned int index = hash(id);
    Node *node = table->buckets[index];
    while (node)
    {
        if (strcmp(node->id, id) == 0)
        {
            return node->value;
        }
        node = node->next;
    }
    return -1; // id not found
}

// Delete from hash table
void deleteNode(HashTable *table, int id)
{
    unsigned int index = hash(id);
    Node *node = table->buckets[index];
    Node *prev = NULL;

    while (node)
    {
        if (strcmp(node->id, id) == 0)
        {
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                table->buckets[index] = node->next;
            }
            free(node->id);
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}