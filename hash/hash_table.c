#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define TABLE_SIZE 100

// node_hash_t structure for chaining
typedef struct node_hash_t
{
    int id;
    void *value;
    struct node_hash_t *next;
} node_hash_t;

// Hash table structure
typedef struct
{
    node_hash_t *buckets[TABLE_SIZE];
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    int balancing;
} hash_table_t;

hash_table_t *create_table()
{
    hash_table_t *table = (hash_table_t *)malloc(sizeof(hash_table_t));
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
void check_balance(hash_table_t *table)
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
node_hash_t *create_node_hash(int id, void *value)
{
    node_hash_t *new_node = (node_hash_t *)malloc(sizeof(node_hash_t));
    new_node->id = id;
    new_node->value = value;
    new_node->next = NULL;
    return new_node;
}

// Insert into hash table
void insert(hash_table_t *table, int id, void *value)
{
    check_balance(table);

    unsigned int index = hash(id);
    node_hash_t *new_node = create_node_hash(id, value);
    new_node->next = table->buckets[index];
    table->buckets[index] = new_node;
}

void *_search(hash_table_t *table, int id)
{
    unsigned int index = hash(id);
    node_hash_t *node = table->buckets[index];
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
void *search(hash_table_t *table, int id)
{
    check_balance(table);
    return _search(table, id);
}

// Search in hash table (read-only please)
void *search_read_only(hash_table_t *table, int id)
{
    return _search(table, id);
}