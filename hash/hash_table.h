#ifndef HASH_TABLE_H
#define HASH_TABLE_H

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

// Function prototypes
HashTable *create_table();
unsigned int hash(int id);
void insert(HashTable *table, int id, void *value);
void* search(HashTable *table, int id);
void* search_read_only(HashTable *table, int id);

#endif // HASH_TABLE_H