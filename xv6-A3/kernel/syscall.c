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
#include "syscall.h"
#include "defs.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  // both tests needed, in case of overflow
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_pause(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_trace(void); /* CMPT 332 Nandish Jha NAJ474 11282001
                                * 2025 change */
extern uint64 sys_mtx_create(void); /* CMPT 332 GROUP 01 Change, Fall 2025 */
extern uint64 sys_mtx_lock(void);   /* CMPT 332 GROUP 01 Change, Fall 2025 */
extern uint64 sys_mtx_unlock(void); /* CMPT 332 GROUP 01 Change, Fall 2025 */
extern uint64 sys_get_num_free_pages(void); /* CMPT 332 GROUP 01 Change,
                                        * Fall 2025 - A3 */
extern uint64 sys_setSchedAttr(void); /* CMPT 332 GROUP 01 Change, Fall 2025 */
extern uint64 sys_getSchedAttr(void); /* CMPT 332 GROUP 01 Change, Fall 2025 */

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_pause]   sys_pause,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace, /* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
[SYS_mtx_create] sys_mtx_create, /* CMPT 332 GROUP 01 Change, Fall 2025 */
[SYS_mtx_lock]   sys_mtx_lock,   /* CMPT 332 GROUP 01 Change, Fall 2025 */
[SYS_mtx_unlock] sys_mtx_unlock, /* CMPT 332 GROUP 01 Change, Fall 2025 */

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
[SYS_get_num_free_pages] sys_get_num_free_pages,
[SYS_setSchedAttr] sys_setSchedAttr, /* CMPT 332 GROUP 01 Change, Fall 2025 */
[SYS_getSchedAttr] sys_getSchedAttr, /* CMPT 332 GROUP 01  Change, Fall 2025 */
};

/* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
static char *syscall_names[] = {
  [SYS_fork]    = "fork",
  [SYS_exit]    = "exit",
  [SYS_wait]    = "wait",
  [SYS_pipe]    = "pipe",
  [SYS_read]    = "read",
  [SYS_kill]    = "kill",
  [SYS_exec]    = "exec",
  [SYS_fstat]   = "fstat",
  [SYS_chdir]   = "chdir",
  [SYS_dup]     = "dup",
  [SYS_getpid]  = "getpid",
  [SYS_sbrk]    = "sbrk",
  [SYS_sleep]   = "sleep",
  [SYS_pause]   = "pause",
  [SYS_uptime]  = "uptime",
  [SYS_open]    = "open",
  [SYS_write]   = "write",
  [SYS_mknod]   = "mknod",
  [SYS_unlink]  = "unlink",
  [SYS_link]    = "link",
  [SYS_mkdir]   = "mkdir",
  [SYS_close]   = "close",
  [SYS_trace]   = "trace", /* CMPT 332 Nandish Jha NAJ474 11282001
                            * 2025 change */
  [SYS_mtx_create] = "mtx_create", /* CMPT 332 GROUP 01 Change, Fall 2025 */
  [SYS_mtx_lock]   = "mtx_lock",   /* CMPT 332 GROUP 01 Change, Fall 2025 */
  [SYS_mtx_unlock] = "mtx_unlock", /* CMPT 332 GROUP 01 Change, Fall 2025 */
  [SYS_get_num_free_pages] = "get_num_free_pages", /* CMPT 332 GROUP 01
                                            * Change, Fall 2025 - A3 */
  [SYS_setSchedAttr] = "setSchedAttr", /* CMPT 332 GROUP 01 Change, Fall 2025 */
  [SYS_getSchedAttr] = "getSchedAttr", /* CMPT 332 GROUP 01 Change, Fall 2025 */
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    /* CMPT 332 Nandish Jha NAJ474 11282001 2025 change */
    uint64 ret = syscalls[num]();
    p->trapframe->a0 = ret;
    if(p->trace_mask & (1 << num)) {
      if(num < NELEM(syscall_names) && syscall_names[num]) {
        printf("%d: syscall %s -> %d\n", p->pid, syscall_names[num], (int)ret);
      } else {
        printf("%d: syscall %d -> %d\n", p->pid, num, (int)ret);
      }
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
