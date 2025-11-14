# xv6 Copy-on-Write Implementation - usertests Fixes

This document summarizes the fixes applied to resolve failing tests in the `usertests` suite for the Copy-on-Write (CoW) fork implementation in xv6.

## Overview

The Copy-on-Write implementation required three critical fixes to pass all `usertests`:

1. **copyout test** - Fixed executable page handling
2. **kernmem test** - Fixed kernel memory access detection  
3. **stacktest** - Fixed page fault return value logic

---

## 1. copyout test - Fixed by modifying `uvmcopy()` in `vm.c`

### Problem
The test was failing with `scause 0x2` (illegal instruction) because CoW was being applied to executable pages.

### Root Cause
The original implementation applied Copy-on-Write to all writable pages without considering that some pages might be both writable and executable. When CoW was applied to executable pages, it removed write permissions but still triggered page faults on instruction fetches, leading to illegal instruction errors.

### Fix
Modified `uvmcopy()` to only apply Copy-on-Write to writable pages by checking the `PTE_W` flag:

```c
// In kernel/vm.c - uvmcopy() function
if(*pte & PTE_W) {
  // Only apply CoW to writable pages
  *pte &= ~PTE_W;  // Remove write permission
  *pte |= PTE_COW; // Mark as CoW
}
```

### Why it worked
Executable pages (like program code) should never be CoW because they're read-only by design. The fix ensures that:
- Only truly writable data pages get CoW treatment
- Executable pages remain unchanged and accessible
- No conflicts between instruction fetching and CoW mechanisms

---

## 2. kernmem test - Fixed by correcting kernel memory detection in `trap.c`

### Problem
The test was hanging because processes trying to access kernel memory weren't being properly killed.

### Root Cause
The kernel memory detection logic was using the wrong boundary:
- Used `MAXVA = 0x4000000000` (256GB) - theoretical maximum virtual address
- Should use `KERNBASE = 0x80000000` (2GB) - where kernel memory actually starts

User processes were accessing addresses like `0x80000000` which is `>= KERNBASE` but `< MAXVA`, so the old check missed them.

### Fix
Changed the kernel memory detection from `va >= MAXVA` to `va >= KERNBASE`:

```c
// In kernel/trap.c - usertrap() function
// Check if accessing kernel virtual address space
if (va >= KERNBASE) {
  setkilled(p);
}
```

### Why it worked
- `KERNBASE = 0x80000000` (2GB) is where kernel memory actually begins in xv6
- Any user process attempting to access addresses >= 2GB should be killed
- This properly catches all kernel memory access attempts

---

## 3. stacktest - Fixed by correcting `vmfault()` return value logic in `trap.c`

### Problem  
The test was hanging because processes trying to access the stack guard page weren't being killed.

### Root Cause
The logic for interpreting `vmfault()` return values was backwards:
- `vmfault()` returns 0 on failure, physical address (non-zero) on success
- The code was checking `!= 0` (success) but then killing the process
- Should check `== 0` (failure) and then kill the process

### Fix
Changed the condition from `!= 0` to `== 0` when checking `vmfault()` return:

```c
// In kernel/trap.c - usertrap() function
// OLD (wrong):
else if (vmfault(p->pagetable, va, (r_scause() == 13) ? 1 : 0) != 0) {
  // vmfault failed, kill the process
  setkilled(p);
}

// NEW (correct):
else if (vmfault(p->pagetable, va, (r_scause() == 13) ? 1 : 0) == 0) {
  // vmfault failed, kill the process  
  setkilled(p);
}
```

### Why it worked
- The stack guard page is mapped but not user-accessible (`PTE_U` bit cleared)
- `vmfault()` correctly detects it's already mapped and returns 0 (failure)  
- Now we properly kill the process when `vmfault()` fails (returns 0)
- This enforces the stack guard protection as intended

---

## Technical Details

### Memory Layout Understanding
- **User space**: 0x0 to `KERNBASE` (2GB)
- **Kernel space**: `KERNBASE` (2GB) to `MAXVA` (256GB)
- **Stack guard**: Mapped page with `PTE_U` cleared to prevent user access

### CoW Implementation Strategy
1. **Page type detection**: Only apply CoW to writable, non-executable pages
2. **Fault handling priority**: 
   - First check kernel memory access → kill
   - Then try CoW for write faults → continue if successful  
   - Finally try lazy allocation → kill if failed
3. **Return value interpretation**: Properly handle success/failure codes

### Test Behavior Explained
- **copyout**: Tests copying data between kernel/user space with CoW pages
- **kernmem**: Forks children that attempt to read kernel memory addresses
- **stacktest**: Forks child that reads below stack to test guard page protection

## Summary

Each fix addressed a different aspect of the Copy-on-Write implementation:

1. **Page type handling** - Don't apply CoW to executable pages
2. **Memory space detection** - Properly detect kernel memory accesses  
3. **Error handling logic** - Correctly interpret `vmfault()` return values

These fixes ensure that CoW only applies where appropriate (writable pages), invalid memory accesses are properly caught and punished, and the overall memory management works correctly with xv6's process model.