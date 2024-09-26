#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#define TABLE_SIZE 10

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
unsigned int hash(int id);
void insert(HashTable *table, int id, void *value);
int search(HashTable *table, int id);
void deleteNode(HashTable *table, int id);

#endif // HASH_TABLE_H