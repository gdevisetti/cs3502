#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define NUM_ACCOUNTS 2
#define INITIAL_BALANCE 1000.00
#define NUM_TRANSFERS 5000

typedef struct {
    int account_id;
    double balance;
    pthread_mutex_t lock;
} Account;

Account accounts[NUM_ACCOUNTS];

void safe_transfer(int from_id, int to_id, double amount) {
    // always grab the lower-numbered account's lock first, no matter which
    // direction the transfer is going - this is what stops the deadlock
    int first  = (from_id < to_id) ? from_id : to_id;
    int second = (from_id < to_id) ? to_id   : from_id;

    pthread_mutex_lock(&accounts[first].lock);
    pthread_mutex_lock(&accounts[second].lock);

    accounts[from_id].balance -= amount;
    accounts[to_id].balance   += amount;

    pthread_mutex_unlock(&accounts[second].lock);
    pthread_mutex_unlock(&accounts[first].lock);
}

void *thread_one(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_TRANSFERS; i++)
        safe_transfer(0, 1, 10.00);
    return NULL;
}

void *thread_two(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_TRANSFERS; i++)
        safe_transfer(1, 0, 10.00);
    return NULL;
}

int main(void) {
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        accounts[i].account_id = i;
        accounts[i].balance = INITIAL_BALANCE;
        pthread_mutex_init(&accounts[i].lock, NULL);
    }

    printf("=== Phase 4: Deadlock Resolution (Lock Ordering) ===\n");
    printf("Same back-and-forth transfer pattern as Phase 3, but now both\n");
    printf("threads grab locks in the same fixed order every time, so\n");
    printf("circular wait just can't happen anymore.\n\n");

    double total_before = accounts[0].balance + accounts[1].balance;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_one, NULL);
    pthread_create(&t2, NULL, thread_two, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    double total_after = accounts[0].balance + accounts[1].balance;

    printf("Completed %d transfers per thread (%d total) with no deadlock.\n",
           NUM_TRANSFERS, NUM_TRANSFERS * 2);
    printf("Final balances: Account 0 = %.2f, Account 1 = %.2f\n",
           accounts[0].balance, accounts[1].balance);
    printf("Total before: %.2f, total after: %.2f (should match - no money lost)\n",
           total_before, total_after);
    printf("Elapsed time: %.4f seconds\n", elapsed);
    printf("\nRun this as many times as you want - it'll never hang like\n");
    printf("Phase 3 did, since lock ordering rules out circular wait entirely.\n");

    for (int i = 0; i < NUM_ACCOUNTS; i++)
        pthread_mutex_destroy(&accounts[i].lock);

    return 0;
}
