/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/

/* CMPT 332 GROUP 01 Change, Fall 2025 */
/* Producer-Consumer using kernel mutex with user-level threads (uthread) */

#include "kernel/types.h"
#include "user/user.h"
#include "uthread.h"

static int shared_data = 0;
static int produced_flag = 0;
static int mutex_id;

/* Forward decl for thread_exit provided by uthread_lib.c */
void thread_exit(void);

static void producer_thread(void)
{
    printf("[thread] Producer: starting\n");
    mtx_lock(mutex_id);
    printf("[thread] Producer: got mutex, writing data\n");
    shared_data = 42;
    printf("[thread] Producer: produced data = %d\n", shared_data);
    mtx_unlock(mutex_id);
    produced_flag = 1;
    /* Yield to let consumer run */
    thread_yield();
    printf("[thread] Producer: done\n");
    thread_exit();
}

static void consumer_thread(void)
{
    printf("[thread] Consumer: starting\n");
    /* Wait until producer has produced something (cooperative yield) */
    while (!produced_flag)
        thread_yield();

    mtx_lock(mutex_id);
    printf("[thread] Consumer: got mutex, reading shared_data = %d\n",\
        shared_data);
    shared_data = shared_data * 2;  // consume/transform the data
    printf("[thread] Consumer: processed data, new value = %d\n", shared_data);
    mtx_unlock(mutex_id);
    printf("[thread] Consumer: released mutex, finished\n");
    thread_exit();
}

int main(void)
{
    printf("Producer-Consumer test using mutex with user-level threads\n");

    /* Create mutex */
    mutex_id = mtx_create(0);
    if (mutex_id < 0) {
        printf("Failed to create mutex\n");
        exit(1);
    }
    printf("Created mutex with ID: %d\n", mutex_id);

    /* Initialize thread system and create threads */
    thread_init();
    thread_create(producer_thread);
    thread_create(consumer_thread);

    /* Run the scheduler until threads exit */
    thread_schedule();

    /* If we return here, all threads exited or no runnable threads remain */
    printf("[main] Final shared_data value: %d\n", shared_data);
    exit(0);
}
/* End CMPT 332 GROUP 01 Change, Fall 2025 */