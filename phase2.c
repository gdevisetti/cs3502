#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define NUM_ACCOUNTS 3
#define NUM_THREADS 4
#define TRANSACTIONS_PER_TELLER 2000
#define INITIAL_BALANCE 1000.00

typedef struct {
    int account_id;
    double balance;
    int transaction_count;
    pthread_mutex_t lock; // one lock per account
} Account;

Account accounts[NUM_ACCOUNTS];

typedef struct {
    int teller_id;
} ThreadArg;

void deposit(int id, double amount) {
    if (pthread_mutex_lock(&accounts[id].lock) != 0) {
        perror("Failed to acquire lock");
        return;
    }
    // only one thread can be in here at a time now
    accounts[id].balance += amount;
    accounts[id].transaction_count++;
    pthread_mutex_unlock(&accounts[id].lock);
}

void withdraw(int id, double amount) {
    if (pthread_mutex_lock(&accounts[id].lock) != 0) {
        perror("Failed to acquire lock");
        return;
    }
    accounts[id].balance -= amount;
    accounts[id].transaction_count++;
    pthread_mutex_unlock(&accounts[id].lock);
}

void *teller_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    int teller_id = targ->teller_id;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();

    for (int i = 0; i < TRANSACTIONS_PER_TELLER; i++) {
        int acc_id = rand_r(&seed) % NUM_ACCOUNTS;
        double amount = (rand_r(&seed) % 100) + 1;
        if (rand_r(&seed) % 2 == 0)
            deposit(acc_id, amount);
        else
            withdraw(acc_id, amount);
    }

    printf("Teller %d finished %d transactions\n", teller_id, TRANSACTIONS_PER_TELLER);
    free(targ);
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        accounts[i].account_id = i;
        accounts[i].balance = INITIAL_BALANCE;
        accounts[i].transaction_count = 0;
        pthread_mutex_init(&accounts[i].lock, NULL); // gotta do this before threads start touching it
    }

    printf("=== Phase 2: Mutex-Protected Threads ===\n");
    printf("Initial balances:\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
        printf("  Account %d: %.2f\n", i, accounts[i].balance);
    printf("\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < NUM_THREADS; i++) {
        ThreadArg *targ = malloc(sizeof(ThreadArg));
        targ->teller_id = i;
        if (pthread_create(&threads[i], NULL, teller_thread, targ) != 0) {
            perror("pthread_create failed");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\nFinal balances (protected):\n");
    int total_transactions = 0;
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        printf("  Account %d: %.2f  (%d transactions)\n",
               i, accounts[i].balance, accounts[i].transaction_count);
        total_transactions += accounts[i].transaction_count;
    }
    printf("\nTotal transactions recorded: %d (expected %d)\n",
           total_transactions, NUM_THREADS * TRANSACTIONS_PER_TELLER);
    printf("Elapsed time: %.4f seconds\n", elapsed);
    printf("Unlike Phase 1, transaction_count always matches the expected\n");
    printf("total, run after run - the mutex prevents lost updates.\n");

    // clean up the locks now that everyone's done with them
    for (int i = 0; i < NUM_ACCOUNTS; i++)
        pthread_mutex_destroy(&accounts[i].lock);

    return 0;
}
