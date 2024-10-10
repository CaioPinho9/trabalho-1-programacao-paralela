#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
#include "hash/hash_table.h"
#include "queue/queue.h"

#define DEPOSIT 0
#define TRANSFER 1
#define BALANCE 2

#define FREE 0
#define BUSY 1

// Number of worker threads processing transactions
int WORKER_THREADS = 5;
// Number of client threads generating transactions
int CLIENT_THREADS = 3;
// Max interval between client transactions
int CLIENT_TRANSACTION_INTERVAL = 10;
// Number of transactions until system shutdown
int TRANSACTION_LIMIT = 100;
// Max interval between processing transactions
int PROCESS_INTERVAL = 3;
// Start balance for new accounts
int START_BALANCE = 1000;
// Max transaction amount
int MAX_TRANSACTION = 1000;
// Decimal precision for transaction amounts
int DECIMAL_PRECISION = 2;

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

typedef struct worker_thread_info_t
{
    thread_pool_t *thread_pool;
    int id;
} worker_thread_info_t;

void process_sleep()
{
    sleep(rand() % PROCESS_INTERVAL);
}

char *format_transaction(transaction_t *transaction)
{
    char *result = malloc(256 * sizeof(char));
    if (transaction == NULL)
    {
        sprintf(result, "[System] checked balance");
    }
    else if (transaction->type == DEPOSIT && transaction->amount > 0)
    {
        sprintf(result, "[Client %d] deposited %.2f", transaction->account_id, transaction->amount);
    }
    else if (transaction->type == DEPOSIT && transaction->amount < 0)
    {
        sprintf(result, "[Client %d] withdrew %.2f", transaction->account_id, -transaction->amount);
    }
    else if (transaction->type == TRANSFER && transaction->amount > 0)
    {
        sprintf(result, "[Client %d] transferred %.2f to Client %d", transaction->account_id, transaction->amount, transaction->receiver_id);
    }
    else
    {
        sprintf(result, "[Client %d] received %.2f from Client %d", transaction->account_id, -transaction->amount, transaction->receiver_id);
    }
    return result;
}

void *worker_thread(void *arg)
{
    worker_thread_info_t *info = (worker_thread_info_t *)arg;
    thread_pool_t *thread_pool = info->thread_pool;
    int id = info->id;

    int state = FREE;
    printf("[Worker thread %d] is Free\n", id);

    pthread_mutex_lock(&thread_pool->mutex);
    thread_pool->thread_count++;
    pthread_mutex_unlock(&thread_pool->mutex);

    while (thread_pool->shutdown == 0)
    {
        // Wait for work
        sem_wait(&thread_pool->work_queue->sem);

        if (thread_pool->shutdown)
            break;

        work_t *work = (work_t *)dequeue(thread_pool->work_queue);

        if (work == NULL)
            break;

        transaction_t *transaction = (transaction_t *)work->arg;

        state = BUSY;
        char *transaction_str = format_transaction(transaction);
        printf("[Worker thread %d] is Busy got [%s]\n", id, transaction_str);
        free(transaction_str);

        pthread_mutex_lock(&thread_pool->mutex);
        thread_pool->working_count++;
        pthread_mutex_unlock(&thread_pool->mutex);

        work->func(transaction);
        free(work);
        state = FREE;
        printf("[Worker thread %d] is Free\n", id);

        // Decrease working count and signal that thread is available
        pthread_mutex_lock(&thread_pool->mutex);
        thread_pool->working_count--;
        pthread_cond_signal(&thread_pool->cond);
        pthread_mutex_unlock(&thread_pool->mutex);
        sem_post(&thread_pool->available_threads);
    }

    // Decrease thread count
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
        worker_thread_info_t *info = (worker_thread_info_t *)malloc(sizeof(worker_thread_info_t));
        info->thread_pool = thread_pool;
        info->id = i;
        pthread_t worker;
        pthread_create(&worker, NULL, worker_thread, info);
        pthread_detach(worker);
    }

    return thread_pool;
}

void *deposit(void *args)
{
    transaction_t *transaction = (transaction_t *)args;

    // Find deposit account
    account_t *deposit_account = (account_t *)search(accounts, transaction->account_id);
    if (deposit_account == NULL)
    {
        printf("[System] Account not found\n");
        free(transaction);
        return NULL;
    }

    // Wait if accounts are being balanced
    pthread_mutex_lock(&accounts->mutex);
    while (accounts->balancing)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }
    // Set flag to indicate that accounts are being used
    accounts->using += 1;
    pthread_mutex_unlock(&accounts->mutex);

    // Deposit to account
    pthread_mutex_lock(&deposit_account->mutex);
    process_sleep();
    deposit_account->balance += transaction->amount;

    if (transaction->amount > 0)
    {
        printf("[System] Deposited %.2f to %s\n", transaction->amount, deposit_account->name);
    }
    else
    {
        printf("[System] Withdraw %.2f from %s\n", -transaction->amount, deposit_account->name);
    }

    pthread_mutex_unlock(&deposit_account->mutex);

    // Unlock account
    pthread_mutex_lock(&accounts->mutex);
    accounts->using -= 1;
    pthread_mutex_unlock(&accounts->mutex);
    pthread_cond_broadcast(&accounts->cond);

    free(transaction);
}

void *transfer(void *args)
{
    transaction_t *transaction = (transaction_t *)args;

    // Find sender and receiver accounts
    account_t *sender = (account_t *)search(accounts, transaction->account_id);
    account_t *receiver = (account_t *)search(accounts, transaction->receiver_id);
    if (sender == NULL || receiver == NULL)
    {
        printf("[System] Account not found\n");
        free(transaction);
        return NULL;
    }

    // Wait if accounts are being balanced
    pthread_mutex_lock(&accounts->mutex);
    while (accounts->balancing)
    {
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }
    // Set flag to indicate that accounts are being used
    accounts->using += 1;
    pthread_mutex_unlock(&accounts->mutex);

    // Make lock order consistent
    if (sender->id > receiver->id)
    {
        account_t *temp = sender;
        sender = receiver;
        receiver = temp;
        transaction->amount *= -1;
    }

    // Lock sender and receiver accounts
    pthread_mutex_lock(&sender->mutex);
    pthread_mutex_lock(&receiver->mutex);

    // Transfer between accounts
    process_sleep();
    sender->balance -= transaction->amount;
    receiver->balance += transaction->amount;

    if (transaction->amount > 0)
    {
        printf("[System] Transferred %.2f from %s to %s\n", transaction->amount, sender->name, receiver->name);
    }
    else
    {
        printf("[System] Transferred %.2f from %s to %s\n", -transaction->amount, receiver->name, sender->name);
    }

    // Unlock accounts
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
    printf("[System] Wait to balancing...\n");
    // Set flag to stop transactions
    accounts->balancing = 1;
    // Wait for all transactions to finish
    while (accounts->using > 0)
    {
        printf("[System] Waiting: %d\n", accounts->using);
        pthread_cond_wait(&accounts->cond, &accounts->mutex);
    }

    // Balance all accounts
    printf("[System] Balancing...\n");
    float total = 0;
    for (size_t i = 0; i < accounts->size; i++)
    {
        account_t *account = (account_t *)search(accounts, i);
        process_sleep();
        printf("[System] Account %s: %.2f\n", account->name, account->balance);
        total += account->balance;
    }
    printf("[System] Total balance: %.2f\n", total);

    // Reset flag and release threads
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

    // Stops when transaction count is above limit, ignoring balance transactions
    while (transaction_count < TRANSACTION_LIMIT + balance_count)
    {
        // Wait for transactions
        sem_wait(&transactions->sem);
        // Wait for available worker thread
        sem_wait(&thread_pool->available_threads);

        transaction_count++;
        transaction_t *transaction = (transaction_t *)dequeue(transactions);

        // Create work for worker thread according to transaction type
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

    thread_pool->shutdown = 1;
    printf("[System] Shutting down...\n");

    // Signal all worker threads
    for (size_t i = 0; i < thread_pool->thread_count; i++)
    {
        sem_post(&thread_pool->work_queue->sem);
    }

    // Wait for all worker threads to finish
    while (thread_pool->thread_count > 0)
    {
        pthread_cond_wait(&thread_pool->cond, &thread_pool->mutex);
    }

    free(thread_pool);
    printf("[System] Shut down.\n");

    return NULL;
}

// Create the same number of accounts as CLIENT_THREADS
void create_accounts()
{
    for (int i = 0; i < CLIENT_THREADS; i++)
    {
        account_t *account = (account_t *)malloc(sizeof(account_t));
        account->id = i;
        account->balance = START_BALANCE;
        account->name = (char *)malloc(10 * sizeof(char));
        pthread_mutex_init(&account->mutex, NULL);
        sprintf(account->name, "Client %d", i);
        insert(accounts, i, account);
        printf("Account: %d %.2f\n", account->id, account->balance);
    }
    printf("\n");
}

void create_random_transaction(int id)
{
    transaction_t *transaction = (transaction_t *)malloc(sizeof(transaction_t));
    transaction->account_id = id;

    // 0: Deposit, 1: Transfer
    transaction->type = rand() % 2;

    if (CLIENT_THREADS == 1)
        transaction->type = DEPOSIT;

    // If transaction type is transfer, choose a random receiver that is not the same as the sender
    if (transaction->type == TRANSFER)
    {
        do
        {
            transaction->receiver_id = rand() % CLIENT_THREADS;
        } while (transaction->receiver_id == transaction->account_id);
    }

    int decimal = pow(10, DECIMAL_PRECISION);
    transaction->amount = (float)(rand() % (MAX_TRANSACTION * decimal)) / decimal;

    // Randomly make the amount negative
    int is_negative = rand() % 2;
    if (is_negative)
        transaction->amount *= -1;

    enqueue(transactions, transaction);

    char *transaction_str = format_transaction(transaction);
    printf("%s\n", transaction_str);
    free(transaction_str);
}

// Send random transactions to the system
void *client_thread(void *arg)
{
    long id = (long)arg;
    // Round up
    for (size_t i = 0; i < (TRANSACTION_LIMIT + CLIENT_THREADS - 1) / CLIENT_THREADS; i++)
    {
        create_random_transaction(id);
        sleep(rand() % CLIENT_TRANSACTION_INTERVAL);
    }
}

int main(int argc, char **argv)
{
    printf("./main [WORKER_THREADS] [CLIENT_THREADS] [CLIENT_TRANSACTION_INTERVAL] [TRANSACTION_LIMIT] [PROCESS_INTERVAL] [MAX_TRANSACTION] [START_BALANCE] [DECIMAL_PRECISION]\n\n");

    switch (argc)
    {
    case 9:
        DECIMAL_PRECISION = atoi(argv[8]);
    case 8:
        START_BALANCE = atoi(argv[7]);
    case 7:
        MAX_TRANSACTION = atoi(argv[6]);
    case 6:
        PROCESS_INTERVAL = atoi(argv[5]);
    case 5:
        TRANSACTION_LIMIT = atoi(argv[4]);
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
    printf("TRANSACTION_LIMIT: %d\n", TRANSACTION_LIMIT);
    printf("PROCESS_INTERVAL: %d\n", PROCESS_INTERVAL);
    printf("MAX_TRANSACTION: %d\n", MAX_TRANSACTION);
    printf("START_BALANCE: %d\n", START_BALANCE);
    printf("DECIMAL_PRECISION: %d\n\n", DECIMAL_PRECISION);

    accounts = create_table();
    transactions = create_queue();

    create_accounts();

    pthread_t pthread;
    for (long i = 0; i < CLIENT_THREADS; i++)
    {
        pthread_create(&pthread, NULL, client_thread, (void *)i);
        pthread_detach(pthread);
    }

    pthread_create(&pthread, NULL, system_thread, NULL);
    pthread_join(pthread, NULL);

    free(accounts);
    free(transactions);

    return 0;
}