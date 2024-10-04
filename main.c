#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
#include "hash/hash_table.h"
#include "queue/queue.h"

#define START_BALANCE 1000
#define MAX_TRANSACTION 10000
#define DECIMAL_PRECISION 2

#define DEPOSIT 0
#define TRANSFER 1
#define BALANCE 2

int WORKER_THREADS = 5;
int CLIENT_THREADS = 3;
int CLIENT_TRANSACTION_INTERVAL = 5;
int TRANSACTION_COUNT = 100;
int PROCESS_INTERVAL = 3;

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
    account_t *deposit_account = (account_t *)search(accounts, transaction->account_id);
    if (deposit_account == NULL)
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

    pthread_mutex_lock(&deposit_account->mutex);
    process_sleep();
    deposit_account->balance += transaction->amount;

    if (transaction->amount > 0)
    {
        printf("Deposited %.2f to %s\n", transaction->amount, deposit_account->name);
    }
    else
    {
        printf("Withdraw %.2f from %s\n", -transaction->amount, deposit_account->name);
    }

    pthread_mutex_unlock(&deposit_account->mutex);

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

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

    if (transaction->amount > 0)
    {
        printf("Transferred %.2f from %s to %s\n", transaction->amount, sender->name, receiver->name);
    }
    else
    {
        printf("Transferred %.2f from %s to %s\n", -transaction->amount, receiver->name, sender->name);
    }

    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

    pthread_mutex_unlock(&receiver->mutex);
    pthread_mutex_unlock(&sender->mutex);

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
    float total = 0;
    for (size_t i = 0; i < accounts->size; i++)
    {
        account_t *account = (account_t *)search(accounts, i);
        process_sleep();
        printf("Account %s has %.2f\n", account->name, account->balance);
        total += account->balance;
    }
    printf("Total balance: %f\n", total);
    accounts->balancing = 0;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

    free(transaction);
}

work_t *create_work(thread_func_t func, void *arg)
{
    work_t *work = (work_t *)malloc(sizeof(work_t));
    work->func = func;
    work->arg = arg;
    return work;
}

void *system_thread(void *arg)
{
    thread_pool_t *thread_pool = create_thread_pool();

    int transaction_count = 0;
    int balance_count = -1;

    while (transaction_count < TRANSACTION_COUNT + balance_count)
    {
        sem_wait(&transactions->sem);
        sem_wait(&thread_pool->available_threads);

        transaction_count++;
        transaction_t *transaction = (transaction_t *)dequeue(transactions);

        work_t *work;
        switch (transaction->type)
        {
        case DEPOSIT:
            work = create_work((thread_func_t)deposit, transaction);
            break;
        case TRANSFER:
            work = create_work((thread_func_t)transfer, transaction);
            break;
        case BALANCE:
            work = create_work((thread_func_t)balance, NULL);
            break;
        default:
            break;
        }

        enqueue(thread_pool->work_queue, work);

        // 11 instead of 10 so it doesn' count the balance transaction
        if (transaction_count % 11 == 0)
        {
            transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
            transaction->type = BALANCE;
            enqueue(transactions, transaction);
            balance_count++;
        }
    }

    // Last balance
    work_t *work = work = create_work((thread_func_t)balance, NULL);
    enqueue(thread_pool->work_queue, work);

    sleep(1);

    // Wait for all worker threads to finish
    while (thread_pool->working_count > 0)
    {
        pthread_cond_wait(&thread_pool->cond, &thread_pool->mutex);
    }

    free(thread_pool);

    return NULL;
}

void create_random_accounts()
{
    for (int i = 0; i < CLIENT_THREADS; i++)
    {
        account_t *account = (account_t *)malloc(sizeof(account_t));
        account->id = i;
        account->balance = START_BALANCE;
        account->name =
            (char *)malloc(10 * sizeof(char));
        pthread_mutex_init(&account->mutex, NULL);
        sprintf(account->name, "Acc%d", i);
        insert(accounts, i, account);
        printf("Account: %d %.2f\n", account->id, account->balance);
    }
    printf("\n");
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
        } while (transaction->receiver_id == transaction->account_id);
    }

    int decimal = pow(10, DECIMAL_PRECISION);
    transaction->amount = (rand() % MAX_TRANSACTION * decimal) / decimal;

    int is_negative = rand() % 2;
    if (is_negative)
        transaction->amount *= -1;

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
    printf("./main WORKER_THREADS CLIENT_THREADS CLIENT_TRANSACTION_INTERVAL TRANSACTION_COUNT PROCESS_INTERVAL\n\n");

    switch (argc)
    {
    case 6:
        PROCESS_INTERVAL = atoi(argv[5]);
    case 5:
        TRANSACTION_COUNT = atoi(argv[4]);
    case 4:
        CLIENT_TRANSACTION_INTERVAL = atoi(argv[3]);
    case 3:
        CLIENT_THREADS = atoi(argv[2]);
    case 2:
        WORKER_THREADS = atoi(argv[1]);
        break;

    default:
        break;
    }

    printf("Configuration:\n");
    printf("WORKER_THREADS: %d\n", WORKER_THREADS);
    printf("CLIENT_THREADS: %d\n", CLIENT_THREADS);
    printf("CLIENT_TRANSACTION_INTERVAL: %d\n", CLIENT_TRANSACTION_INTERVAL);
    printf("TRANSACTION_COUNT: %d\n", TRANSACTION_COUNT);
    printf("PROCESS_INTERVAL: %d\n\n", PROCESS_INTERVAL);

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