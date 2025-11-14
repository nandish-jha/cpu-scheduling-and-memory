/*
AAKASH JANA AAJ284 11297343
NANDISH JHA NAJ474 11282001
*/
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

/* CMPT 332 GROUP 01 Change, Fall 2025 */
struct mutex mutexes[NMUTEX];
struct spinlock mutex_table_lock;
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Global ready queue for EDF scheduling
struct ready_queue rq;
struct spinlock rq_lock;  // Lock for ready queue operations
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Global ready queue for EDF scheduling
struct ready_queue rq;
struct spinlock rq_lock;  // Lock for ready queue operations
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

static void freeproc(struct proc *p);

extern void forkret(void);
/* CMPT 332 GROUP 01 Change, Fall 2025 */
// Forward declarations for ready queue helper functions
static void rq_insert(struct proc *p);
static void rq_remove(struct proc *p);
static struct proc* rq_dequeue(void) __attribute__((unused));
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

extern void forkret(void);
/* CMPT 332 GROUP 01 Change, Fall 2025 */
// Forward declarations for ready queue helper functions
static void rq_insert(struct proc *p);
static void rq_remove(struct proc *p);
static struct proc* rq_dequeue(void) __attribute__((unused));
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }

  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  // Initialize mutex table
  initlock(&mutex_table_lock, "mutex_table");
  for(int i = 0; i < NMUTEX; i++) {
    initlock(&mutexes[i].lock, "mutex");
    mutexes[i].locked = 0;
    mutexes[i].used = 0;
    mutexes[i].owner = 0;
  }
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */

  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  // Initialize ready queue
  initlock(&rq_lock, "ready_queue");
  rq.count = 0;
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  p->deadline = 0;
  p->priority = 0;
  p->start_time = 0;  /* default: start immediately */
  p->in_rq = 0;  /* not yet in ready queue */
  /* CMPT 332 GROUP 01 Change, Fall 2025 */

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  p->deadline = 0;
  p->priority = 0;
  p->start_time = 0;
  p->in_rq = 0;
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  p->cwd = namei("/");

  p->state = RUNNABLE;
  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  rq_insert(p);
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */

  release(&p->lock);
}

// Shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Ready queue helper functions
// These must be called with the global process lock held (scheduler lock)

// Insert a process into the ready queue in sorted order
// (deadline, then priority)
// Only inserts if start_time <= current ticks and not already in queue
// Called with rq_lock to be acquired by this function
static void
rq_insert(struct proc *p)
{
  // printf("Inserting process %d into ready queue with deadline %d"
  //        " and priority %d\n", p->pid, p->deadline, p->priority);
  acquire(&rq_lock);
  
  // Check if already in queue to avoid double insertion
  if(p->in_rq) {
    release(&rq_lock);
    return;
  }
  
  // Check if process's start time has arrived
  // Safely read ticks - check if we already hold tickslock to avoid deadlock
  int now;
  if(holding(&tickslock)) {
    // We're probably in a timer interrupt, just use ticks directly
    now = ticks;
  } else {
    acquire(&tickslock);
    now = ticks;
    release(&tickslock);
  }
  
  // If start_time has not yet arrived, do not insert;
  // leave for deferred activation
  if(p->start_time > now) {
    release(&rq_lock);
    return;
  }
  
  int i;
  // Safety check - don't overflow the queue
  if(rq.count >= NPROC) {
    release(&rq_lock);
    panic("ready queue overflow");
  }
  
  // Find the correct position to insert based on (deadline, priority) ordering
  i = rq.count;
  while(i > 0 && (rq.procs[i-1]->deadline > p->deadline ||
                   (rq.procs[i-1]->deadline == p->deadline && 
                    rq.procs[i-1]->priority > p->priority))) {
    rq.procs[i] = rq.procs[i-1];
    i--;
  }
  
  rq.procs[i] = p;
  p->in_rq = 1;  /* mark as in ready queue */
  rq.count++;
  release(&rq_lock);
}

// Remove a specific process from the ready queue
// Called with global process lock held
static void
rq_remove(struct proc *p)
{
  int i;
  // printf("Removing process %d from ready queue\n", p->pid);
  acquire(&rq_lock);
  // Find the process in the queue
  for(i = 0; i < rq.count; i++) {
    if(rq.procs[i] == p) {
      // Shift remaining processes forward
      for(int j = i; j < rq.count - 1; j++) {
        rq.procs[j] = rq.procs[j+1];
      }
      rq.count--;
      p->in_rq = 0;  /* mark as not in ready queue */
      release(&rq_lock);
      return;
    }
  }
  release(&rq_lock);
}

// Remove and return the first (earliest deadline) process from the ready queue
// Called with global process lock held
static struct proc*
rq_dequeue(void)
{
  struct proc *p;
  // acquire(&rq_lock);
  
  if(rq.count == 0)
    return 0;
  
  p = rq.procs[0];
  p->in_rq = 0;  /* mark as not in ready queue */
  
  // Shift remaining processes forward
  for(int i = 0; i < rq.count - 1; i++) {
    rq.procs[i] = rq.procs[i+1];
  }
  rq.count--;
  // release(&rq_lock);
  return p;
}
/* End CMPT 332 GROUP 01 Change, Fall 2025 */

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));
  
  np->trace_mask = p->trace_mask; /* CMPT 332 Nandish Jha NAJ474 11282001
                                   * 2025 change */
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  np->deadline = p->deadline; /* setting properties while p->lock held */
  np->priority = p->priority;
  /* CMPT 332 GROUP 01 Change, Fall 2025 */

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  rq_insert(np);
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  // Release all user memory for this process.
  proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  // ----------
  
  acquire(&p->lock);

  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  // Remove from ready queue if present
  if(p->state == RUNNABLE) {
    rq_remove(p);
  }
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    /* CMPT 332 GROUP 01 Change, Fall 2025 */
    // Deferred activation: scan process table for processes
    // whose start_time has arrived
    // and add them to the ready queue
    acquire(&tickslock);
    int now = ticks;
    release(&tickslock);
    
    struct proc *candidate;
    int i;
    for(i = 0; i < NPROC; i++) {
      candidate = &proc[i];
      acquire(&candidate->lock);
      // Check if process is RUNNABLE, not in queue yet,
      // and start_time has arrived
      if(candidate->state == RUNNABLE && !candidate->in_rq &&
         candidate->start_time <= now) {
        // Call rq_insert outside the lock to avoid deadlock
        // rq_insert will acquire rq_lock internally
        release(&candidate->lock);
        rq_insert(candidate);
      } else {
        release(&candidate->lock);
      }
    }
    
    // Use EDF scheduling: pop earliest deadline process from ready queue
    acquire(&rq_lock);
    p = rq_dequeue();
    release(&rq_lock);
    
    if(p != 0) {
      // Acquire process lock to safely modify its state
      acquire(&p->lock);
      
      // Process should be RUNNABLE; verify and run it
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      // If not RUNNABLE, just continue the loop to check queue again
      release(&p->lock);
    } else {
      // Ready queue is empty; nothing to run
      // Stop running on this core until an interrupt.
      asm volatile("wfi");
    }
    /* End CMPT 332 GROUP 01 Change, Fall 2025 */
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{ 
  struct proc *p = myproc();
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
  acquire(&p->lock);
  p->state = RUNNABLE;
  release(&p->lock);
  rq_insert(p);
  acquire(&p->lock);
  sched();
  release(&p->lock);
  /* CMPT 332 GROUP 01 Change, Fall 2025 */
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Sleep on channel chan, releasing condition lock lk.
// Re-acquires lk when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  // Remove from ready queue if RUNNABLE
  if(p->state == RUNNABLE) {
    rq_remove(p);
  }
  /* End CMPT 332 GROUP 01 Change, Fall 2025 */

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on channel chan.
// Caller should hold the condition lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
        rq_insert(p);
        /* End CMPT 332 GROUP 01 Change, Fall 2025 */
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
        /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
        rq_insert(p);
        /* End CMPT 332 GROUP 01 Change, Fall 2025 */
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

/* CMPT 332 GROUP 01 Change, Fall 2025 */
// Create a mutex lock and return its ID
uint64
sys_mtx_create(void)
{
  int locked;
  
  // Get the initial lock state from the argument
  argint(0, &locked);
  
  acquire(&mutex_table_lock);
  
  // Find an unused mutex slot
  for(int i = 0; i < NMUTEX; i++) {
    if(!mutexes[i].used) {
      mutexes[i].used = 1;
      mutexes[i].locked = locked ? 1 : 0;
      mutexes[i].owner = locked ? myproc() : 0;
      release(&mutex_table_lock);
      return i;  // Return mutex ID
    }
  }
  
  release(&mutex_table_lock);
  return -1;  // No available mutex slots
}

// Lock a mutex (blocks until acquired)
uint64
sys_mtx_lock(void)
{
  int lock_id;
  
  // Get the mutex ID from the argument
  argint(0, &lock_id);
  
  // Validate mutex ID
  if(lock_id < 0 || lock_id >= NMUTEX) {
    return -1;
  }
  
  acquire(&mutex_table_lock);
  
  // Check if mutex exists
  if(!mutexes[lock_id].used) {
    release(&mutex_table_lock);
    return -1;
  }
  
  release(&mutex_table_lock);
  
  // Acquire the specific mutex lock
  acquire(&mutexes[lock_id].lock);
  
  // Wait until the mutex is unlocked
  while(mutexes[lock_id].locked) {
    sleep(&mutexes[lock_id], &mutexes[lock_id].lock);
  }
  
  // Lock the mutex
  mutexes[lock_id].locked = 1;
  mutexes[lock_id].owner = myproc();
  
  release(&mutexes[lock_id].lock);
  return 0;
}

// Unlock a mutex
uint64
sys_mtx_unlock(void)
{
  int lock_id;
  
  // Get the mutex ID from the argument
  argint(0, &lock_id);
  
  // Validate mutex ID
  if(lock_id < 0 || lock_id >= NMUTEX) {
    return -1;
  }
  
  acquire(&mutexes[lock_id].lock);
  
  // Check if mutex exists and is locked
  if(!mutexes[lock_id].used || !mutexes[lock_id].locked) {
    release(&mutexes[lock_id].lock);
    return -1;
  }
  
  // Unlock the mutex
  mutexes[lock_id].locked = 0;
  mutexes[lock_id].owner = 0;
  
  // Wake up any waiting processes
  wakeup(&mutexes[lock_id]);
  
  release(&mutexes[lock_id].lock);
  return 0;
}
/* End CMPT 332 GROUP 01 Change, Fall 2025 */
