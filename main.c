#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "hash/hash_table.h"
#include "queue/queue.h"

typedef struct
{
    int id;
    float balance;
    char *name;
    pthread_mutex_t mutex;
} Account;

void *system_thread(void *arg)
{
    return NULL;
}

int main(int argc, char **argv)
{
    HashTable accounts;

    return 0;
}