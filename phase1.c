#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define NUM_ACCOUNTS 3
#define NUM_THREADS 4
#define TRANSACTIONS_PER_TELLER 2000
#define INITIAL_BALANCE 1000.00

typedef struct {
    int account_id;
    double balance;
    int transaction_count;
} Account;

Account accounts[NUM_ACCOUNTS];

typedef struct {
    int teller_id;
} ThreadArg;

void *teller_thread(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    int teller_id = targ->teller_id;

    // give each thread its own random seed so they don't all roll the same numbers
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();

    for (int i = 0; i < TRANSACTIONS_PER_TELLER; i++) {
        int acc_id = rand_r(&seed) % NUM_ACCOUNTS;
        double amount = (rand_r(&seed) % 100) + 1;
        int is_deposit = rand_r(&seed) % 2;

        // grab the balance, wait a tiny bit, then write it back - no lock here on purpose
        double current = accounts[acc_id].balance;
        usleep(1); // just widening the gap so two threads are more likely to collide

        if (is_deposit)
            accounts[acc_id].balance = current + amount;
        else
            accounts[acc_id].balance = current - amount;

        accounts[acc_id].transaction_count++;
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
    }

    printf("=== Phase 1: Basic Threads (No Synchronization) ===\n");
    printf("Initial balances:\n");
    for (int i = 0; i < NUM_ACCOUNTS; i++)
        printf("  Account %d: %.2f\n", i, accounts[i].balance);
    printf("\n");

    // spin up the tellers
    for (int i = 0; i < NUM_THREADS; i++) {
        ThreadArg *targ = malloc(sizeof(ThreadArg));
        targ->teller_id = i;
        if (pthread_create(&threads[i], NULL, teller_thread, targ) != 0) {
            perror("pthread_create failed");
            exit(1);
        }
    }

    // wait for everyone to finish
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("\nFinal balances (subject to race conditions):\n");
    double total = 0;
    int total_ops = 0;
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        printf("  Account %d: %.2f  (%d transactions recorded)\n",
               i, accounts[i].balance, accounts[i].transaction_count);
        total += accounts[i].balance;
        total_ops += accounts[i].transaction_count;
    }
    printf("\nTotal across accounts: %.2f\n", total);
    printf("Total transactions recorded: %d (expected %d)\n",
           total_ops, NUM_THREADS * TRANSACTIONS_PER_TELLER);
    printf("Run this program several times in a row - the final total will\n");
    printf("differ between runs because of lost updates (the race condition).\n");

    return 0;
}
