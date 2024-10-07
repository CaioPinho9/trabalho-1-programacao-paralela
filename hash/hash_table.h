#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#define TABLE_SIZE 100

typedef struct node_hash_t
{
    int id;
    void *value;
    struct node_hash_t *next;
} node_hash_t;

typedef struct
{
    node_hash_t *buckets[TABLE_SIZE];
    int balancing;
    int using;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} hash_table_t;

hash_table_t *create_table();
void insert(hash_table_t *table, int id, void *value);
void *search(hash_table_t *table, int id);

#endif // HASH_TABLE_H