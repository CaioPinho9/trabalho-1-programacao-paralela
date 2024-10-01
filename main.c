#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include "hash/hash_table.h"
#include "queue/queue.h"

#define WORKER_THREADS 5
#define CLIENT_THREADS 3
#define CLIENT_TRANSACTION_INTERVAL 5
#define TRANSACTION_COUNT 100
#define PROCESS_INTERVAL 3

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
    sem_t available_threads;
    int working_count;
    int thread_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} thread_pool_t;

void process_sleep()
{
    sleep(rand() % PROCESS_INTERVAL);
}

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
        free(work);

        pthread_mutex_lock(&thread_pool->mutex);
        thread_pool->working_count--;
        pthread_cond_signal(&thread_pool->cond);
        pthread_mutex_unlock(&thread_pool->mutex);
        sem_post(&thread_pool->available_threads);
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
    sem_init(&thread_pool->available_threads, 0, WORKER_THREADS);
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
        free(transaction);
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
    process_sleep();
    account->balance += transaction->amount;
    printf("Deposited %.2f to %s\n", transaction->amount, account->name);

    pthread_mutex_unlock(&account->mutex);

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_cond_broadcast(&accounts->cond);
    pthread_mutex_unlock(&accounts->mutex);

    free(transaction);
}

void *transfer(void *args)
{
    transaction_t *transaction = (transaction_t *)args;
    account_t *sender = (account_t *)search(accounts, transaction->account_id);
    account_t *receiver = (account_t *)search(accounts, transaction->receiver_id);
    if (sender == NULL || receiver == NULL)
    {
        printf("Account not found\n");
        free(transaction);
        return NULL;
    }

    pthread_mutex_lock(&accounts->mutex);
    while (accounts->balancing)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }
    accounts->using += 1;
    pthread_mutex_unlock(&accounts->mutex);

    if (sender->id > receiver->id)
    {
        account_t *temp = sender;
        sender = receiver;
        receiver = temp;
        transaction->amount *= -1;
    }

    pthread_mutex_lock(&sender->mutex);
    pthread_mutex_lock(&receiver->mutex);

    process_sleep();
    sender->balance -= transaction->amount;
    receiver->balance += transaction->amount;
    printf("Transferred %.2f from %s to %s\n", transaction->amount, sender->name, receiver->name);

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

    pthread_mutex_unlock(&sender->mutex);
    pthread_mutex_unlock(&receiver->mutex);

    free(transaction);
}

void *balance(void *args)
{
    transaction_t *transaction = (transaction_t *)args;

    pthread_mutex_lock(&accounts->mutex);
    printf("Wait to balancing...\n");
    accounts->balancing = 1;
    while (accounts->using > 0)
    {
        printf("Waiting: %d\n", accounts->using);
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }

    printf("Balancing...\n");
    int total = 0;
    for (size_t i = 0; i < accounts->size; i++)
    {
        account_t *account = (account_t *)search(accounts, i);
        process_sleep();
        printf("Account %s has %.2f\n", account->name, account->balance);
        total += account->balance;
    }
    printf("Total balance: %d\n", total);
    accounts->balancing = 0;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

    free(transaction);
}

void *system_thread(void *arg)
{
    thread_pool_t *thread_pool = create_thread_pool();

    int transaction_count = 0;
    int balance_count = 0;

    while (transaction_count < TRANSACTION_COUNT + balance_count)
    {
        // printf("Transaction count: %d\n", transaction_count);
        sem_wait(&transactions->sem);
        sem_wait(&thread_pool->available_threads);

        transaction_count++;
        transaction_t *transaction = (transaction_t *)dequeue(transactions);

        work_t *work = (work_t *)malloc(sizeof(work_t));
        switch (transaction->type)
        {
        case DEPOSIT:
            work->func = (thread_func_t)deposit;
            break;
        case TRANSFER:
            work->func = (thread_func_t)transfer;
            break;
        case BALANCE:
            work->func = (thread_func_t)balance;
            break;
        default:
            break;
        }
        work->arg = transaction;

        enqueue(thread_pool->work_queue, work);

        if (transaction_count % 10 == 0)
        {
            transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
            transaction->type = BALANCE;
            enqueue(transactions, transaction);
            balance_count++;
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
    for (int i = 0; i < CLIENT_THREADS; i++)
    {
        account_t *account = (account_t *)malloc(sizeof(account_t));
        account->id = i;
        account->balance = 1000;
        account->name =
            (char *)malloc(10 * sizeof(char));
        pthread_mutex_init(&account->mutex, NULL);
        sprintf(account->name, "Acc%d", i);
        insert(accounts, i, account);
        printf("Account: %d %.2f\n", account->id, account->balance);
    }
}

void create_random_transaction(int id)
{
    transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
    transaction->account_id = id;

    transaction->type = rand() % 2;

    if (transaction->type == TRANSFER)
    {
        do
        {
            transaction->receiver_id = rand() % CLIENT_THREADS;
        } while (transaction->receiver_id == transaction->account_id && transaction->receiver_id != 0);
    }

    transaction->amount = 100;

    // printf("Transaction: %d %d %d %.2f\n", transaction->account_id, transaction->receiver_id, transaction->type, transaction->amount);
    enqueue(transactions, transaction);
}

void *client_thread(void *arg)
{
    long id = (long)arg;
    for (size_t i = 0; i < TRANSACTION_COUNT / CLIENT_THREADS; i++)
    {
        create_random_transaction(id);
        sleep(rand() % CLIENT_TRANSACTION_INTERVAL);
    }
}

int main(int argc, char **argv)
{
    accounts = create_table();
    transactions = create_queue();

    create_random_accounts();

    pthread_t pthread;
    for (long i = 0; i < CLIENT_THREADS; i++)
    {
        pthread_create(&pthread, NULL, client_thread, (void *)i);
        pthread_detach(pthread);
    }

    pthread_create(&pthread, NULL, system_thread, NULL);
    pthread_join(pthread, NULL);

    return 0;
}