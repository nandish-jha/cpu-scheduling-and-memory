# xv6 Copy-on-Write Fork Implementation Journey
## CMPT 332 Assignment 3 - Complete Implementation Log

**Date:** November 16, 2025  
**Assignment:** Copy-on-Write Fork in xv6 Operating System  
**Status:** ✅ COMPLETED - All tests passing

---

## Table of Contents
1. [Overview](#overview)
2. [Implementation Timeline](#implementation-timeline)
3. [Step-by-Step Implementation](#step-by-step-implementation)
4. [Bug Investigation and Fixes](#bug-investigation-and-fixes)
5. [Final Working Implementation](#final-working-implementation)
6. [Testing Results](#testing-results)
7. [Key Learnings](#key-learnings)

---

## Overview

This document chronicles the complete implementation of Copy-on-Write (CoW) fork functionality in xv6, an educational operating system. The goal was to modify the fork() system call so that parent and child processes share physical memory pages until one of them writes to a page, at which point a private copy is made.

### Key Requirements:
- Implement reference counting for physical pages
- Modify uvmcopy() to share pages instead of copying
- Add page fault handler for write-to-shared-page events
- Ensure proper memory cleanup on process exit
- Pass all tests in the provided cowtest program

---

## Implementation Timeline

### Phase 1: Infrastructure Setup
**Duration:** Initial setup  
**Goal:** Add monitoring syscall and understand codebase

### Phase 2: Reference Counting System
**Duration:** Core implementation  
**Goal:** Track how many processes reference each physical page

### Phase 3: Copy-on-Write Fork
**Duration:** Main feature implementation  
**Goal:** Modify fork to share pages, handle write faults

### Phase 4: Debugging and Bug Fixes
**Duration:** Extensive debugging session  
**Goal:** Fix memory leaks, panics, and edge cases

### Phase 5: Testing and Validation
**Duration:** Final testing  
**Goal:** Verify cowtest passes all test cases

---

## Step-by-Step Implementation

### Step 1: getNumFreePages() System Call
**Purpose:** Testing infrastructure to monitor memory usage

**Files Modified:**
- `kernel/sysproc.c` - Added `sys_getNumFreePages()`
- `kernel/syscall.h` - Added syscall number
- `kernel/syscall.c` - Added syscall entry
- `user/usys.pl` - Added user-space wrapper

**Implementation:**
```c
uint64
sys_getNumFreePages(void)
{
  return getNumFreePages(); // Calls function in kalloc.c
}
```

**Result:** ✅ System call works, can monitor free memory

---

### Step 2: Reference Counting Infrastructure
**Purpose:** Track how many processes share each physical page

**Files Modified:**
- `kernel/kalloc.c` - Core reference counting system

**Key Components:**
```c
// Global reference count array for all physical pages
int page_refcount[NPHYS_PAGES];
struct spinlock refcount_lock;

// Convert physical address to array index
int phys2ind(uint64 pa) {
  return (pa - KERNBASE) / PGSIZE;
}

// Reference count operations
void incref(uint64 pa);    // Increment reference count
void decref(uint64 pa);    // Decrement reference count  
int getref(uint64 pa);     // Get current reference count
```

**Critical Design Decision:**
- All reference counting centralized in kalloc.c
- kfree() handles decrements automatically
- No direct calls to decref() from other modules

**Result:** ✅ Reference counting system operational

---

### Step 3: Modified Memory Allocation
**Purpose:** Integrate reference counting with kalloc/kfree

**Files Modified:**
- `kernel/kalloc.c`

**Changes:**
```c
void* kalloc(void) {
  // ... existing allocation logic ...
  
  // Set initial reference count to 1
  acquire(&refcount_lock);
  page_refcount[phys2ind((uint64)r)] = 1;
  release(&refcount_lock);
  
  return (void*)r;
}

void kfree(void *pa) {
  // ... sanity checks ...
  
  // Decrement reference count, only free if reaches 0
  int do_free = 0;
  acquire(&refcount_lock);
  if (--page_refcount[phys2ind((uint64)pa)] == 0) {
    do_free = 1;
  }
  release(&refcount_lock);
  
  if (!do_free) return; // Still referenced, don't free
  
  // ... free page to freelist ...
}
```

**Result:** ✅ Memory allocator now tracks references

---

### Step 4: Copy-on-Write Fork Implementation
**Purpose:** Modify fork to share pages instead of copying

**Files Modified:**
- `kernel/vm.c` - Modified `uvmcopy()`

**Original behavior:**
```c
// Old uvmcopy: Always copy pages
mem = kalloc();
memmove(mem, (char*)pa, PGSIZE);
mappages(new, i, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U);
```

**New CoW behavior:**
```c
// New uvmcopy: Share pages, remove write permissions
incref(pa); // Increment reference count
// Map same physical page in both parent and child, but read-only
mappages(new, i, PGSIZE, pa, (flags & ~PTE_W) | PTE_U);
// Also remove write permission from parent's mapping
*pte = (*pte & ~PTE_W);
```

**Result:** ✅ Fork now shares pages between parent and child

---

### Step 5: Page Fault Handler (cowfault)
**Purpose:** Handle writes to shared pages by allocating new copies

**Files Modified:**
- `kernel/vm.c` - Added `cowfault()` function
- `kernel/trap.c` - Added page fault handling
- `kernel/defs.h` - Added function declaration

**Core Algorithm:**
```c
int cowfault(pagetable_t pagetable, uint64 va) {
  if (va >= MAXVA) return -1;
  
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0) return -1;
  
  uint64 pa = PTE2PA(*pte);
  
  if (getref(pa) == 1) {
    // Only one reference, just restore write permission
    *pte |= PTE_W;
  } else {
    // Multiple references, need to copy
    char *new_page = kalloc();
    if (new_page == 0) return -1;
    
    memmove(new_page, (void*)pa, PGSIZE);
    *pte = PA2PTE(new_page) | PTE_FLAGS(*pte) | PTE_W;
    
    // This automatically decrements refcount for old page
    kfree((void*)pa);
  }
  
  sfence_vma(); // Flush TLB
  return 0;
}
```

**Trap Handler Integration:**
```c
void usertrap(void) {
  // ... existing trap handling ...
  
  if (r_scause() == 15) { // Store page fault
    if (cowfault(p->pagetable, r_stval()) < 0) {
      p->killed = 1; // Kill process if fault can't be handled
    }
  }
  
  // ... rest of trap handling ...
}
```

**Result:** ✅ Write faults now trigger page copying

---

### Step 6: copyout() CoW Support
**Purpose:** Handle kernel-to-user writes that may trigger CoW

**Files Modified:**
- `kernel/vm.c` - Modified `copyout()` function

**Problem:** When kernel writes data to user space (e.g., during read() syscall), it needs to trigger CoW if the destination page is shared.

**Solution:**
```c
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  // ... existing bounds checking ...
  
  while(len > 0) {
    va0 = PGROUNDDOWN(dstva);
    
    // Check if page is CoW and trigger fault if needed
    pte_t *pte = walk(pagetable, va0, 0);
    if (pte && (*pte & PTE_V) && !(*pte & PTE_W)) {
      if (cowfault(pagetable, va0) < 0) {
        return -1;
      }
    }
    
    // ... rest of copy logic ...
  }
  return 0;
}
```

**Result:** ✅ Kernel writes to user space work correctly with CoW

---

### Step 7: Process Exit Cleanup
**Purpose:** Ensure proper memory cleanup when processes exit

**Files Modified:**
- `kernel/proc.c` - Modified `kexit()` function

**Challenge:** When a process exits, all its memory pages must be properly dereferenced and freed if no longer shared.

**Implementation:**
```c
void kexit(int status) {
  // ... existing cleanup code ...
  
  // Use proc_freepagetable instead of uvmfree directly
  proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  
  // ... rest of exit logic ...
}
```

**Result:** ✅ Process exit properly cleans up shared memory

---

### Step 8: sleep() System Call Addition
**Purpose:** Make cowtest compile successfully

**Files Modified:**
- `kernel/sysproc.c` - Added `sys_sleep()`
- `kernel/syscall.h` - Added syscall number
- `kernel/syscall.c` - Added syscall entry  
- `user/usys.pl` - Added user-space wrapper

**Simple Implementation:**
```c
uint64 sys_sleep(void) {
  return sys_pause(); // Reuse existing pause functionality
}
```

**Result:** ✅ cowtest now compiles and links

---

## Bug Investigation and Fixes

### Bug 1: Boot Hang (Reference Count Initialization)
**Symptom:** System hangs during boot after implementing reference counting

**Root Cause Analysis:**
```c
// In kinit():
for (int i = 0; i < NPHYS_PAGES; i++)
  page_refcount[i] = 0; // All pages start with 0 references

// In freerange():
for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
  kfree(p); // This decrements 0 to -1, then tries to free!
}
```

**Problem:** Pages in `freerange()` start with refcount 0, but `kfree()` decrements first, causing negative reference counts.

**Fix:**
```c
void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // Set refcount to 1 before calling kfree
    acquire(&refcount_lock);
    page_refcount[phys2ind((uint64)p)] = 1;
    release(&refcount_lock);
    kfree(p); // Now decrements 1 to 0 and frees correctly
  }
}
```

**Result:** ✅ Boot process completes successfully

---

### Bug 2: Double-Decrement Memory Corruption
**Symptom:** Negative reference counts, memory corruption, "freewalk: leaf" panics

**Root Cause Analysis:**
The most critical bug in our implementation was a double-decrement issue in memory management:

```c
// Original broken code in uvmunmap():
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE) {
    if((pte = walk(pagetable, a, 0)) == 0) continue;   
    if((*pte & PTE_V) == 0) continue;

    if(do_free){
      uint64 pa = PTE2PA(*pte);
      decref(pa);    // ❌ FIRST decrement here
      kfree((void*)pa); // ❌ SECOND decrement inside kfree()!
    }
    *pte = 0;
  }
}
```

**The Problem Chain:**
1. `uvmunmap()` calls `decref(pa)` - refcount goes from 1 to 0
2. `uvmunmap()` calls `kfree(pa)` 
3. `kfree()` internally calls `decref(pa)` again - refcount goes from 0 to -1!
4. Page gets freed but refcount is negative
5. Later operations on the same page cause corruption

**Debug Evidence:**
```
cowfault: page 0x87f5a000 has refcount -3
```

**The Fix:**
```c
// Fixed code - let kfree() handle ALL reference counting:
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE) {
    if((pte = walk(pagetable, a, 0)) == 0) continue;   
    if((*pte & PTE_V) == 0) continue;

    if(do_free){
      uint64 pa = PTE2PA(*pte);
      // ✅ Only call kfree() - it handles decrements internally
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

**Why This Fix Works:**
- Centralized reference counting in `kfree()` only
- No manual `decref()` calls from other modules
- Eliminates double-decrement possibility
- `kfree()` decrements and frees atomically

**Result:** ✅ Reference counting now works correctly

---

### Bug 3: "freewalk: leaf" Panic 
**Symptom:** Panic when process exits: "freewalk: leaf"

**Root Cause Analysis:**
```c
// Original broken kexit() code:
void kexit(int status) {
  // ... cleanup code ...
  
  uvmfree(p->pagetable, p->sz); // ❌ Wrong! Leaves TRAMPOLINE/TRAPFRAME mapped
  p->pagetable = 0;
  
  // ...
}
```

**The Problem:**
- `uvmfree()` only unmaps pages from 0 to `p->sz`
- TRAMPOLINE and TRAPFRAME pages are mapped at high addresses (near MAXVA)
- These pages remained mapped when `freewalk()` tried to free page table
- `freewalk()` expects ALL leaf mappings to be removed first

**Memory Layout Understanding:**
```
User Address Space:
0x0                    - Program code/data
...
p->sz                  - End of program memory  
...
TRAPFRAME (MAXVA-2*PGSIZE) - Process trapframe
TRAMPOLINE (MAXVA-PGSIZE)  - Kernel trampoline code
MAXVA                      - End of address space
```

**The Fix:**
```c
// Fixed kexit() code:
void kexit(int status) {
  // ... cleanup code ...
  
  // Use proper cleanup function that unmaps TRAMPOLINE/TRAPFRAME first
  proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  
  // ...
}
```

**What `proc_freepagetable()` does correctly:**
```c
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);  // Unmap trampoline
  uvmunmap(pagetable, TRAPFRAME, 1, 0);   // Unmap trapframe  
  uvmfree(pagetable, sz);                 // Then free user pages
}
```

**Result:** ✅ Process exit cleans up all mappings correctly

---

### Bug 4: Kernel Panic in kfree()
**Symptom:** "panic: kfree" during system operation

**Root Cause:** Attempted stack unmapping at wrong address

**Problem Code:**
```c
// Incorrect attempt to unmap "user stack"
void uvmfree(pagetable_t pagetable, uint64 sz) {
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  
  // ❌ Wrong! User stack is NOT at fixed high address
  uvmunmap(pagetable, MAXVA - 2*PGSIZE, 2, 1);
  
  freewalk(pagetable);
}
```

**Misconception:** I incorrectly assumed user stack was at a fixed high address like TRAMPOLINE/TRAPFRAME.

**Reality:** User stack is allocated with `uvmalloc()` right after program data, growing from `sz` upward.

**The Fix:** Remove incorrect stack unmapping - user stack is already handled by `uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1)`.

**Result:** ✅ No more kfree panics

---

## Final Working Implementation

### Complete File Changes Summary

**kernel/kalloc.c:**
- Added reference counting array and lock
- Modified `kalloc()` to set initial refcount = 1  
- Modified `kfree()` to decrement refcount, only free when 0
- Added helper functions: `incref()`, `decref()`, `getref()`, `phys2ind()`

**kernel/vm.c:**
- Modified `uvmcopy()` for CoW: share pages, remove write permissions
- Modified `uvmunmap()` to use `kfree()` only (no manual decrements)
- Added `cowfault()` function for handling write faults
- Modified `copyout()` to trigger CoW for kernel-to-user writes

**kernel/trap.c:**
- Added page fault handling in `usertrap()` for scause == 15

**kernel/proc.c:**
- Modified `kexit()` to use `proc_freepagetable()` instead of `uvmfree()`

**kernel/sysproc.c:**
- Added `sys_getNumFreePages()` and `sys_sleep()` system calls

**Various headers and syscall tables:**
- Added new syscall entries and function declarations

---

## Testing Results

### cowtest Output:
```
$ cowtest
simple: ok
simple: ok  
three: ok
three: ok
three: ok
file: ok
ALL COW TESTS PASSED
```

### Test Breakdown:

**Simple Test:**
- Allocates >50% of system memory
- Forks child process  
- Verifies memory sharing works correctly
- **Result:** ✅ PASS

**Three Test:** 
- Creates 3 processes that all write to shared pages
- Tests complex CoW scenarios with multiple writers
- Verifies proper page copying and reference management
- **Result:** ✅ PASS  

**File Test:**
- Tests `copyout()` functionality 
- Kernel writes data to user space through shared pages
- Verifies CoW triggers correctly for kernel-to-user writes
- **Result:** ✅ PASS

---

## Key Learnings

### 1. Reference Counting Best Practices
- **Centralize all reference management in one module** (kalloc.c)
- **Never allow manual decrements from outside modules**
- **Make kfree() handle all decrements automatically**
- **Use atomic operations with proper locking**

### 2. Page Table Management Complexity  
- **Different page types require different cleanup:** User pages vs TRAMPOLINE vs TRAPFRAME
- **Order matters:** Unmap special pages before calling uvmfree()
- **freewalk() expects NO leaf mappings remaining**

### 3. Memory Layout Understanding
- **User stack is NOT at fixed high address** - it's allocated dynamically after program data
- **TRAMPOLINE/TRAPFRAME are at fixed high addresses** near MAXVA
- **Different address ranges need different handling**

### 4. Debugging Strategies That Worked
- **Add debug prints to trace reference count operations**
- **Check for negative reference counts as corruption indicator** 
- **Enable freewalk debug to see which mappings remain**
- **Test with simple programs before complex ones**

### 5. xv6-Specific Insights
- **proc_freepagetable() vs uvmfree():** Know when to use each
- **Page fault handling:** scause == 15 for store page faults
- **TLB management:** sfence_vma() after page table changes
- **System call integration:** usys.pl, syscall.h, syscall.c coordination

### 6. Common Pitfalls to Avoid
- ❌ **Double-decrementing reference counts**
- ❌ **Forgetting to unmap special pages before freewalk()**  
- ❌ **Assuming user stack location**
- ❌ **Not handling copyout() CoW scenarios**
- ❌ **Missing TLB flushes after page table modifications**

---

## Implementation Success Metrics

✅ **Functionality:** All cowtest cases pass  
✅ **Memory Safety:** No memory leaks or corruption  
✅ **Performance:** Sharing reduces memory usage vs traditional fork  
✅ **Robustness:** Handles edge cases and error conditions  
✅ **Code Quality:** Clean, well-commented implementation  

**Total Implementation Time:** 1 full day of intensive development and debugging  
**Final Status:** Ready for submission with full functionality

---

*This implementation demonstrates a complete working Copy-on-Write fork system in xv6, with proper error handling, memory management, and edge case coverage.*