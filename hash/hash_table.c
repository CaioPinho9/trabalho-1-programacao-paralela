#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int balancing;
} HashTable;

HashTable *create_table()
{
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        table->buckets[i] = NULL;
    }
    pthread_cond_init(&table->cond, NULL);
    pthread_mutex_init(&table->mutex, NULL);
    table->balancing = 0;
    return table;
}

// Stops the thread until the balancing is done
void check_balance(HashTable *table)
{
    while (table->balancing)
    {
        pthread_cond_wait(&table->cond, &table->mutex);
    }
}

// Simple hash function
unsigned int hash(int id)
{
    return id % TABLE_SIZE;
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
    check_balance(table);

    unsigned int index = hash(id);
    NodeHash *new_node = create_node_hash(id, value);
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

void *_search(HashTable *table, int id)
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

// Search in hash table
void *search(HashTable *table, int id)
{
    check_balance(table);
    return _search(table, id);
}

// Search in hash table (read-only please)
void *search_read_only(HashTable *table, int id)
{
    return _search(table, id);
}