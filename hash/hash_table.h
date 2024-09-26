#ifndef HASH_TABLE_H
#define HASH_TABLE_H

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
} hash_table_t;

// Function prototypes
hash_table_t *create_table();
unsigned int hash(int id);
void insert(hash_table_t *table, int id, void *value);
void *search(hash_table_t *table, int id);
void *search_read_only(hash_table_t *table, int id);

#endif // HASH_TABLE_H