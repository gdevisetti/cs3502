#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define NUM_ACCOUNTS 2
#define INITIAL_BALANCE 1000.00
#define NUM_TRANSFERS 5
#define DEADLOCK_TIMEOUT_SEC 5

typedef struct {
    int account_id;
    double balance;
    pthread_mutex_t lock;
} Account;

Account accounts[NUM_ACCOUNTS];

// watchdog checks this to see if anything's actually happening
volatile int completed_transfers = 0;

void transfer(int from_id, int to_id, double amount) {
    printf("Thread %lu: Attempting transfer from %d to %d\n",
           (unsigned long)pthread_self(), from_id, to_id);

    pthread_mutex_lock(&accounts[from_id].lock);
    printf("Thread %lu: Locked account %d\n", (unsigned long)pthread_self(), from_id);

    usleep(100000); // 100ms pause - gives the other thread time to grab its own lock too

    printf("Thread %lu: Waiting for account %d\n", (unsigned long)pthread_self(), to_id);
    pthread_mutex_lock(&accounts[to_id].lock);

    // if we made it here, we got lucky and both locks came through
    accounts[from_id].balance -= amount;
    accounts[to_id].balance += amount;
    __sync_fetch_and_add(&completed_transfers, 1);

    printf("Thread %lu: Transfer complete (%d -> %d)\n",
           (unsigned long)pthread_self(), from_id, to_id);

    pthread_mutex_unlock(&accounts[to_id].lock);
    pthread_mutex_unlock(&accounts[from_id].lock);
}

// locks accounts[0] first, then accounts[1]
void *thread_one(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_TRANSFERS; i++)
        transfer(0, 1, 10.00);
    return NULL;
}

// locks accounts[1] first, then accounts[0] - opposite order, this is the problem
void *thread_two(void *arg) {
    (void)arg;
    for (int i = 0; i < NUM_TRANSFERS; i++)
        transfer(1, 0, 10.00);
    return NULL;
}

void *watchdog_thread(void *arg) {
    (void)arg;
    int last_seen = -1;
    int stall_seconds = 0;

    while (1) {
        sleep(1);
        int now = completed_transfers;

        if (now >= NUM_TRANSFERS * 2)
            return NULL; // both threads wrapped up normally, nothing to report

        if (now == last_seen) {
            stall_seconds++;
        } else {
            stall_seconds = 0;
            last_seen = now;
        }

        if (stall_seconds >= DEADLOCK_TIMEOUT_SEC) {
            printf("\n*** WATCHDOG: no progress for %d seconds. "
                   "Possible deadlock detected! ***\n", DEADLOCK_TIMEOUT_SEC);
            printf("*** Completed transfers: %d / %d ***\n", now, NUM_TRANSFERS * 2);
            printf("*** Both threads are almost certainly stuck in circular\n");
            printf("*** wait on each other's account lock. Exiting. ***\n");
            exit(1);
        }
    }
}

int main(void) {
    for (int i = 0; i < NUM_ACCOUNTS; i++) {
        accounts[i].account_id = i;
        accounts[i].balance = INITIAL_BALANCE;
        pthread_mutex_init(&accounts[i].lock, NULL);
    }

    printf("=== Phase 3: Deadlock Demonstration ===\n");
    printf("Thread one transfers 0 -> 1 while thread two transfers 1 -> 0.\n");
    printf("Each locks its own 'from' account first, then waits on the\n");
    printf("other one - classic circular wait. Watchdog thread is watching.\n\n");

    pthread_t t1, t2, watchdog;
    pthread_create(&t1, NULL, thread_one, NULL);
    pthread_create(&t2, NULL, thread_two, NULL);
    pthread_create(&watchdog, NULL, watchdog_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(watchdog, NULL);

    printf("\nBoth threads completed without deadlock this run.\n");
    printf("Final balances: Account 0 = %.2f, Account 1 = %.2f\n",
           accounts[0].balance, accounts[1].balance);
    printf("(Deadlock depends on scheduling timing, so it might not happen\n");
    printf(" every single run - try again a couple times if it doesn't.)\n");

    for (int i = 0; i < NUM_ACCOUNTS; i++)
        pthread_mutex_destroy(&accounts[i].lock);

    return 0;
}
