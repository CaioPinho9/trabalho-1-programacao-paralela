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

Queue *transactions;
HashTable *accounts;

typedef struct Account
{
    int id;
    float balance;
    char *name;
    pthread_mutex_t mutex;
} Account;

typedef struct Transaction
{
    int account_id;
    int receiver_id;
    int type;
    float amount;
} Transaction;

void *worker_thread(void *arg)
{
    while (1)
    {
        sem_wait(&transactions->sem);
        Transaction *transaction = (Transaction *)dequeue(transactions);
        if (transaction == NULL)
        {
            continue;
        }

        switch (transaction->type)
        {
        case DEPOSIT:
            deposit(transaction);
            break;

        case TRANSFER:
            transfer(transaction);
            break;

        case BALANCE:
            balance(transaction);
            break;

        default:
            break;
        }
    }

    return NULL;
}

void deposit(Transaction *transaction)
{
    Account *account = (Account *)search(accounts, transaction->account_id);
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