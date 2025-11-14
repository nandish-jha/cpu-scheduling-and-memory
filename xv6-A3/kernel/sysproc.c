#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

// Prototype declarations
uint64 sys_pause(void);
int getNumFreePages(void);

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Syscall handler function that calls the getNumFreePages kernel function
uint64
sys_get_num_free_pages(void)
{
  return getNumFreePages();
}

/* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  struct proc *p = myproc();
  p->trace_mask = mask;
  return 0;
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_sleep(void)
{
  return sys_pause();
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/* CMPT 332 GROUP 01 Change, Fall 2025 */
/* Set scheduling attributes for the current process */
uint64
sys_setSchedAttr(void)
{
  uint64 uaddr;
  struct sched_attr attr;
  struct proc *p;

  /* Read argument 0 as a user pointer to struct sched_attr */
  argaddr(0, &uaddr);

  /* Copy the struct from user space into schedule attr */
  if(copyin(myproc()->pagetable, (char *)&attr, uaddr,
            sizeof(struct sched_attr)) < 0) {
    return -1;
  }

  /* Get the current process */
  p = myproc();

  /* With the process lock held, update the scheduling attributes */
  acquire(&p->lock);
  p->deadline = attr.deadline;
  p->priority = attr.priority;
  p->start_time = attr.start_time;  /* set the start time from the attribute */
  release(&p->lock);

  return 0;
}

/* Get scheduling attributes for the current process */
uint64
sys_getSchedAttr(void)
{
  uint64 uaddr;
  struct sched_attr attr;
  struct proc *p;

  /* Read argument 0 as user pointer to struct sched_attr */
  argaddr(0, &uaddr);

  /* Get the current process */
  p = myproc();

  /* With the process lock held, read the scheduling attributes */
  acquire(&p->lock);
  attr.deadline = p->deadline;
  attr.priority = p->priority;
  attr.start_time = p->start_time;  /* read the start time */
  release(&p->lock);

  /* Copy the struct back to user space */
  if(copyout(p->pagetable, uaddr, (char *)&attr,
             sizeof(struct sched_attr)) < 0) {
    return -1;
  }

  return 0;
}
/* CMPT 332 GROUP 01 Change, Fall 2025 */

