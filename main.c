#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include "hash/hash_table.h"
#include "queue/queue.h"
#include "main.h"

#define WORKER_THREADS 5
#define ACCOUNT_COUNT 10

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
    pthread_cond_t cond;
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

void *worker_thread(void *arg)
{
    thread_pool_t *thread_pool = (thread_pool_t *)arg;

    pthread_mutex_lock(&thread_pool->mutex);
    thread_pool->thread_count++;
    pthread_mutex_unlock(&thread_pool->mutex);

    while (1)
    {
        sem_wait(&thread_pool->work_queue->sem);

        work_t *work = (work_t *)dequeue(thread_pool->work_queue);

        if (work == NULL)
            break;

        pthread_mutex_lock(&thread_pool->mutex);
        thread_pool->working_count++;
        pthread_mutex_unlock(&thread_pool->mutex);

        work->func(work->arg);
        printf("Working count: %d\n", thread_pool->working_count);
        free(work);
        printf("Working count: %d\n", thread_pool->working_count);

        pthread_mutex_lock(&thread_pool->mutex);
        thread_pool->working_count--;
        pthread_mutex_unlock(&thread_pool->mutex);
    }

    pthread_mutex_lock(&thread_pool->mutex);
    thread_pool->thread_count--;
    if (thread_pool->thread_count == 0)
    {
        pthread_cond_signal(&thread_pool->cond);
    }
    pthread_mutex_unlock(&thread_pool->mutex);

    return NULL;
}

thread_pool_t *create_thread_pool()
{
    thread_pool_t *thread_pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    thread_pool->work_queue = create_queue();
    thread_pool->working_count = 0;
    thread_pool->thread_count = 0;
    pthread_mutex_init(&thread_pool->mutex, NULL);
    pthread_cond_init(&thread_pool->cond, NULL);
    thread_pool->shutdown = 0;

    for (int i = 0; i < WORKER_THREADS; i++)
    {
        pthread_t worker;
        pthread_create(&worker, NULL, worker_thread, thread_pool);
        pthread_detach(worker);
    }

    return thread_pool;
}

void *deposit(void *args)
{
    transaction_t *transaction = (transaction_t *)args;
    account_t *account = (account_t *)search(accounts, transaction->account_id);
    if (account == NULL)
    {
        printf("Account not found\n");
        return NULL;
    }

    pthread_mutex_lock(&accounts->mutex);
    while (accounts->balancing)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }
    accounts->using += 1;
    pthread_mutex_unlock(&accounts->mutex);

    pthread_mutex_lock(&accounts->mutex);

    sleep(rand() % 3);
    account->balance += transaction->amount;
    printf("Deposited %.2f to %s\n", transaction->amount, account->name);

    pthread_mutex_unlock(&account->mutex);

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;

    if (accounts->using == 0)
    {
        pthread_cond_broadcast(&accounts->cond);
    }
    pthread_mutex_unlock(&accounts->mutex);

    free(transaction);
}

void *transfer(void *args)
{
    transaction_t *transaction = (transaction_t *)args;
    account_t *account = (account_t *)search(accounts, transaction->account_id);
    account_t *receiver = (account_t *)search(accounts, transaction->receiver_id);
    if (account == NULL || receiver == NULL)
    {
        printf("Account not found\n");
        return NULL;
    }

    pthread_mutex_lock(&accounts->mutex);
    while (accounts->balancing)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }
    accounts->using += 1;
    pthread_mutex_unlock(&accounts->mutex);

    pthread_mutex_lock(&account->mutex);
    while (pthread_mutex_trylock(&receiver->mutex) != 0)
    {
        pthread_cond_wait(&accounts->cond, &account->mutex);
    }

    sleep(rand() % 3);
    account->balance -= transaction->amount;
    receiver->balance += transaction->amount;
    printf("Transferred %.2f from %s to %s\n", transaction->amount, account->name, receiver->name);

    pthread_mutex_unlock(&account->mutex);
    pthread_mutex_unlock(&receiver->mutex);
    pthread_cond_signal(&account->cond);
    pthread_cond_signal(&receiver->cond);

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_signal(&accounts->cond);

    free(transaction);
}

void *balance(void *args)
{
    transaction_t *transaction = (transaction_t *)args;

    pthread_mutex_lock(&accounts->mutex);
    while (accounts->using > 0)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }

    accounts->balancing = 1;
    printf("Balancing...\n");
    for (size_t i = 0; i < accounts->size; i++)
    {
        account_t *account = (account_t *)search(accounts, i);
        sleep(rand() % 3);
        printf("Account %s has %.2f\n", account->name, account->balance);
    }
    accounts->balancing = 0;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);
    free(transaction);
}

void *system_thread(void *arg)
{
    thread_pool_t *thread_pool = create_thread_pool();

    int transaction_count = 0;

    while (transaction_count < 100)
    {
        printf("Transaction count: %d\n", transaction_count);
        sem_wait(&transactions->sem);
        transaction_count++;

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
        enqueue(thread_pool->work_queue, work);

        if (transaction_count % 10 == 0)
        {
            transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
            transaction->type = 3;
            enqueue(transactions, transaction);
        }
    }

    thread_pool->shutdown = 1;
    pthread_cond_broadcast(&thread_pool->cond);

    while (thread_pool->working_count > 0)
    {
        pthread_cond_wait(&thread_pool->cond, &thread_pool->mutex);
    }

    return NULL;
}

void create_random_accounts()
{
    for (int i = 0; i < ACCOUNT_COUNT; i++)
    {
        account_t *account = (account_t *)malloc(sizeof(account_t));
        account->id = i;
        account->balance = rand() % 1000;
        account->name =
            (char *)malloc(10 * sizeof(char));
        pthread_mutex_init(&account->mutex, NULL);
        pthread_cond_init(&account->cond, NULL);
        sprintf(account->name, "Acc%d", i);
        insert(accounts, i, account);
        printf("Account: %d %.2f\n", account->id, account->balance);
    }
}

void create_random_transaction()
{
    transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
    transaction->account_id = rand() % ACCOUNT_COUNT;
    do
    {
        transaction->receiver_id = rand() % ACCOUNT_COUNT;
    } while (transaction->receiver_id != transaction->account_id);

    transaction->type = rand() % 3;
    transaction->amount = rand() % 1000;
    printf("Transaction: %d %d %d %.2f\n", transaction->account_id, transaction->receiver_id, transaction->type, transaction->amount);
    enqueue(transactions, transaction);
}

int main(int argc, char **argv)
{
    accounts = create_table();
    transactions = create_queue();

    create_random_accounts();

    for (int i = 0; i < 100; i++)
    {
        create_random_transaction();
    }

    pthread_t pthread;
    pthread_create(&pthread, NULL, system_thread, NULL);
    pthread_join(pthread, NULL);

    return 0;
}