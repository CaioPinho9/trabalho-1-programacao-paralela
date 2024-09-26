#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#define TABLE_SIZE 10

// Node structure for chaining
typedef struct Node {
    int id;
    int value;
    struct Node* next;
} Node;

// Hash table structure
typedef struct {
    Node* buckets[TABLE_SIZE];
} HashTable;

// Function prototypes
unsigned int hash(int id);
void insert(HashTable* table, int id, int value);
int search(HashTable* table, int id);
void deleteNode(HashTable* table, int id);

#endif // HASH_TABLE_H