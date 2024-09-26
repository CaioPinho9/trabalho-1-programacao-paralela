#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include "hash/hash_table.h"
#include "queue/queue.h"

#define WORKER_THREADS 5

#define DEPOSIT 0
#define TRANSFER 1
#define BALANCE 2

queue_t *transactions;
hash_table_t *accounts;

typedef struct account_t
{
    int id;
    float balance;
    char *name;
    pthread_mutex_t mutex;
} account_t;

typedef struct transaction_t
{
    int account_id;
    int receiver_id;
    int type;
    float amount;
} transaction_t;

typedef void (*thread_func_t)(void *arg);
typedef struct work_t
{
    thread_func_t func;
    void *arg;
} work_t;

typedef struct thread_pool_t
{
    queue_t *work_queue;
    int working_count;
    int thread_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} thread_pool_t;

void *system_thread(void *arg)
{
    thread_pool_t thread_pool = create_thread_pool();
    while (1)
    {
        sem_wait(&transactions->sem);

        if (thread_pool.shutdown)
            break;

        transaction_t *transaction = (transaction_t *)dequeue(transactions);

        work_t *work = (work_t *)malloc(sizeof(work_t));
        switch (transaction->type)
        {
        case DEPOSIT:
            work->func = (thread_func_t)deposit;
            work->arg = transaction;
            break;
        case TRANSFER:
            work->func = (thread_func_t)transfer;
            work->arg = transaction;
            break;
        case BALANCE:
            work->func = (thread_func_t)balance;
            work->arg = transaction;
            break;
        default:
            break;
        }

        
    }
}

void *worker_thread(void *arg)
{
    while (1)
    {
    }

    return NULL;
}

void deposit(transaction_t *transaction)
{
    account_t *account = (account_t *)search(accounts, transaction->account_id);
    if (account == NULL)
    {
        printf("Account not found\n");
        return;
    }

    pthread_mutex_lock(&account->mutex);
    account->balance += transaction->amount;
    pthread_mutex_unlock(&account->mutex);
    printf("Deposited %.2f to %s\n", transaction->amount, account->name);
}

int main(int argc, char **argv)
{
    accounts = create_table();
    transactions = create_queue();

    pthread_t workers[WORKER_THREADS];

    for (int i = 0; i < WORKER_THREADS; i++)
    {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    for (int i = 0; i < WORKER_THREADS; i++)
    {
        pthread_join(workers[i], NULL);
    }

    return 0;
}