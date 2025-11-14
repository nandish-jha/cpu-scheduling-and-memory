/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "uthread.h"

/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192
#define MAX_THREAD  4

struct regi {
  /* control registers */
  uint64 ra;
  uint64 sp;

  /* callee registers */
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct regi registers;        /* bringing the struct into this struct */
};
struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
extern void thread_switch(uint64, uint64);
              
void 
thread_init(void)
{
  /* main() is thread 0, which will make the first invocation to 
   thread_schedule().  it needs a stack so that the first thread_switch() can
   save thread 0's state.  thread_schedule() won't run the main thread ever
   again, because its state is set to RUNNING, and thread_schedule() selects
   a RUNNABLE thread. */
  current_thread = &all_thread[0];
  current_thread->state = RUNNING;
  
  // Initialize main thread's register context to safe values
  // (The main thread uses the system stack, so we don't allocate one)
  memset(&current_thread->registers, 0, sizeof(struct regi));
}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)&t->registers, (uint64)&current_thread->registers);
  } else
    next_thread = 0;
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  /* YOUR CODE HERE */
  
  // Set up stack pointer to point to top of this thread's stack
  // (stacks grow downward, so start at the end)
  t->registers.sp = (uint64)&t->stack[STACK_SIZE-1];
  
  // Set return address to the function this thread should execute
  t->registers.ra = (uint64)func;
  
  // Initialize all saved registers to 0 (instead of 12 lines)
  // memset(&t->registers.s0, 0, 12 * sizeof(uint64));
}

void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

void
thread_exit(void)
{
  current_thread->state = FREE;
  thread_schedule();
}
