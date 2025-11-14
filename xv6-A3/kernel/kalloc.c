// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Reference count for each physical page
#define NPHYS_PAGES ((PHYSTOP - KERNBASE) / PGSIZE)
static int page_refcount[NPHYS_PAGES];
static struct spinlock refcount_lock;

// Convert physical address to page_refcount[] index
static inline int
phys2ind(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Increment the reference count for a physical page
void
incref(uint64 pa)
{
  acquire(&refcount_lock);
  page_refcount[phys2ind(pa)]++;
  release(&refcount_lock);
}

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Decrement the reference count for a physical page
void
decref(uint64 pa)
{
  acquire(&refcount_lock);
  page_refcount[phys2ind(pa)]--;
  release(&refcount_lock);
}

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Get the reference count for a physical page
int
getref(uint64 pa)
{
  int ref;

  acquire(&refcount_lock);
  ref = page_refcount[phys2ind(pa)];
  release(&refcount_lock);
  
  return ref;
}

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  initlock(&refcount_lock, "refcount");
  for (int i = 0; i < NPHYS_PAGES; i++)
    page_refcount[i] = 0;
  // ----------

  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
    // set reference count to 1 so kfree will free it
    acquire(&refcount_lock);
    page_refcount[phys2ind((uint64)p)] = 1;
    release(&refcount_lock);
    // ----------
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  // Sanity check here
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
  int do_free = 0;
  acquire(&refcount_lock);
  // decrement refcount and if reaches 0, freeing is allowed
  if (--page_refcount[phys2ind((uint64)pa)] == 0) {
    do_free = 1;
  }
  release(&refcount_lock);
  // if page is still being used/reffed, don't do anything
  if (!do_free)
    return;
  // ----------
    
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk

    /* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
    acquire(&refcount_lock);
    page_refcount[phys2ind((uint64)r)] = 1; // single owner
    release(&refcount_lock);
  }
    return (void*)r;
}

/* CMPT 332 GROUP 01 Change, Fall 2025 - A3 */
// Return the number of free 4096-byte pages of physical memory.
// Traverses the free list and counts available pages.
int
getNumFreePages(void)
{
  struct run *r;
  int count = 0;

  acquire(&kmem.lock);
  for(r = kmem.freelist; r; r = r->next)
    count++;
  release(&kmem.lock);

  return count;
}
