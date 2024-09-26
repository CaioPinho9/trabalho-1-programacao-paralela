#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 100

// NodeHash structure for chaining
typedef struct NodeHash
{
    int id;
    void *value;
    struct NodeHash *next;
} NodeHash;

// Hash table structure
typedef struct
{
    NodeHash *buckets[TABLE_SIZE];
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
NodeHash *create_node_hash(int id, void *value)
{
    NodeHash *new_node = (NodeHash *)malloc(sizeof(NodeHash));
    new_node->id = id;
    new_node->value = value;
    new_node->next = NULL;
    return new_node;
}

// Insert into hash table
void insert(HashTable *table, int id, void *value)
{
    unsigned int index = hash(id);
    NodeHash *new_node = create_node_hash(id, value);
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

// Search in hash table
void *search(HashTable *table, int id)
{
    unsigned int index = hash(id);
    NodeHash *node = table->buckets[index];
    while (node)
    {
        if (node->id == id)
        {
            return node->value;
        }
        node = node->next;
    }
    return NULL; // id not found
}

// Delete from hash table
void deleteNode(HashTable *table, int id)
{
    unsigned int index = hash(id);
    NodeHash *node = table->buckets[index];
    NodeHash *prev = NULL;

    while (node)
    {
        if (node->id == id)
        {
            if (prev)
            {
                prev->next = node->next;
            }
            else
            {
                table->buckets[index] = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}