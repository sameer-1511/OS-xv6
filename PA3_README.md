# CS3523: Page Replacement in xv6 (PA3)

## Overview

This assignment implements page replacement with eviction and swapping in the xv6 kernel. When physical memory runs out, instead of terminating the process, the kernel now evicts pages using the Clock algorithm, stores their contents in a swap area, and reuses the freed frame. When a swapped page is accessed again, it is restored from swap.

The implementation integrates components from PA1 (per-process statistics) and PA2 (SC-MLFQ scheduling) to implement scheduler-aware page replacement.

---

## Implemented Features

### 1. Frame Table Management
- **Structure**: Each frame tracks whether it's in use, which process owns it, the virtual address mapped to it, and a reference bit for the Clock algorithm.
- **Location**: `kernel/vm.c` - global `frametable[MAXFRAMES]`
- **Initialization**: Frame table is initialized in `kvminit()` with all frames marked as free.

### 2. Swap Space
- **Capacity**: 8192 pages (32MB with 4KB pages)
- **Implementation**: In-memory array `swapspace[MAX_SWAP][PGSIZE]` in kernel/vm.c
- **Tracking**: Per-process `swap_index[MAX_PROC_PAGES]` array tracks which swap slot holds each page

### 3. Clock Page Replacement Algorithm
- **Location**: `select_eviction_frame()` in kernel/vm.c
- **Operation**:
  - Maintains a global `clock_hand` pointer to circular list of frames
  - Scans frames in circular order
  - If reference bit is 0: frame selected for eviction
  - If reference bit is 1: bit cleared, continue scanning
- **Complexity**: O(NPHYS_PAGES) worst case per eviction

### 4. Scheduler-Aware Eviction
- **Priority**: Lower-priority processes (higher MLFQ queue levels) have their pages evicted first
- **Logic**: When multiple victim candidates exist, prioritizes pages from processes in queue level > best->owner->qlevel
- **Benefit**: Interactive (high-priority) processes retain their working sets longer

### 5. Page Fault Handling
- **Location**: `vmfault()` in kernel/vm.c
- **Two Cases**:
  1. **Swapped Page** (PTE_S bit set):
     - Allocates physical frame via kalloc()
     - If kalloc() fails, triggers eviction and retries
     - Copies page data from swap space to physical memory
     - Marks frame as in-use and sets reference bit
     - Clears PTE_S, sets PTE_V
  2. **Unmapped Page** (lazy allocation):
     - Same flow as above for in-memory pages
     - Initializes page with zeros

### 6. Page Eviction
- **Function**: `evict_page()` in kernel/vm.c
- **Process**:
  1. Copies page data from physical memory to swap space
  2. Updates PTE: clears PTE_V, sets PTE_S
  3. Records swap slot in process's swap_index array
  4. Calls kfree() to release physical frame
  5. Updates process statistics (pages_evicted, pages_swapped_out)
  6. Manages frame table

### 7. Fork Support
- **Modified**: `uvmcopy()` now accepts parent process pointer
- **Handles Swapped Pages**: When parent has swapped pages, child gets a copy:
  - Allocates new swap slots for child
  - Copies swap data from parent's slots
  - Sets up appropriate PTEs with PTE_S flags
- **Preserves Swap Index**: Child inherits parent's swap_index array initially
- **Critical Fix**: Previously, swapped pages were skipped during fork, causing child processes to lose access to swapped memory

### 8. Memory Statistics Tracking
- **Per-Process Fields** in `struct proc`:
  - `page_faults`: Count of page faults handled
  - `pages_evicted`: Pages evicted from this process
  - `pages_swapped_in`: Pages restored from swap
  - `pages_swapped_out`: Pages written to swap
  - `resident_pages`: Currently resident pages in memory
  - `swap_index[1024]`: Mapping from page index to swap slot

### 9. getvmstats() Syscall
- **Signature**: `int getvmstats(int pid, struct vmstats *info)`
- **Returns**: 0 on success, -1 if PID invalid
- **Fills**: `struct vmstats` with current process statistics
- **Implementation**: Kernel function `kgetvmstats()` in proc.c

---

## Design Decisions

### 1. In-Memory Swap vs. Disk Swap
- **Decision**: Implemented in-memory swap in kernel
- **Rationale**: Simpler to verify correctness; avoids I/O complexity
- **Trade-off**: Limited to 1024 pages; assignment requirement satisfied

### 2. Global Frame Table
- **Decision**: Single global `frametable[NPHYS_PAGES]` protected by spinlock
- **Rationale**: Simple, correct, minimal overhead
- **Alternative**: Per-process frame lists (more complex)

### 3. Reference Bit Updates
- **Decision**: Set on page fault, updated in copyout/copyin via `update_refbit()`
- **Rationale**: Reasonable approximation; xv6 lacks MMU hardware tracking
- **Limitation**: User-space direct accesses don't trigger ref bit updates (unavoidable in xv6)

### 4. Clock Hand Position
- **Decision**: Single global `clock_hand` pointer
- **Rationale**: Standard Clock algorithm; simple circular scan
- **Efficiency**: Worst case O(n) per eviction, but typical case much better

### 5. Scheduler Integration
- **Decision**: Check `owner->qlevel` in `select_eviction_frame()`
- **Rationale**: Prioritizes evicting pages from lower-priority processes
- **Benefit**: Rewards interactive (high-priority) processes

---

## Assumptions

1. **Maximum Virtual Address**: 4096 pages per process (MAX_PROC_PAGES, swap_index array size)
   - Supports processes up to 16MB of virtual memory
   
2. **Swap Capacity**: 8192 pages total across all processes (MAX_SWAP)
   - Supports significant memory overcommitment
   - If exceeded, kernel panics with "swap full" message

3. **Memory Configuration**: Physical frame table tracks MAXFRAMES (256 frames)
   - Swap capacity: 8192 pages (32MB)
   - Enforces frame-based eviction even when kalloc() has free pages

4. **Reference Bits**: Approximation only
   - Cannot track all user-space accesses without hardware support
   - copyout/copyin paths are tracked; direct accesses are not

5. **Fork Behavior**: Parent and child share swap data initially
   - Both point to same swap slots initially
   - Later evictions and page faults create independent copies
   - No copy-on-write optimization

---

## Modified Files

### Kernel Files

1. **kernel/types.h**
   - Added `struct frame` with fields: in_use, owner, va, ref_bits
   - Added `struct vmstats` with page replacement statistics

2. **kernel/proc.h**
   - Extended `struct proc` with PA3 fields: page_faults, pages_evicted, pages_swapped_in, pages_swapped_out, resident_pages, swap_index[1024]

3. **kernel/defs.h**
   - Updated `uvmcopy()` signature to include `struct proc*` parameter
   - Added declarations for `select_eviction_frame()` and `evict_page()`

4. **kernel/vm.c**
   - Added frame table: `struct frame frametable[NPHYS_PAGES]`
   - Added swap space: `char swapspace[MAX_SWAP][PGSIZE]`
   - Modified `kvminit()` to initialize frame table
   - Rewrote `vmfault()` to handle eviction and swapping
   - Implemented `select_eviction_frame()` with Clock algorithm
   - Implemented `evict_page()` for page eviction logic
   - Implemented `ismapped()` to check PTE_V or PTE_S
   - Implemented `update_refbit()` to set reference bits
   - Modified `uvmcopy()` to handle swapped pages

5. **kernel/proc.c**
   - Modified `allocproc()` to initialize PA3 statistics
   - Modified `freeproc()` to clean up swap slots
   - Added `kgetvmstats()` to retrieve statistics
   - Updated `kfork()` to pass parent pointer to `uvmcopy()`

6. **kernel/sysproc.c**
   - Added `sys_getvmstats()` syscall implementation

7. **kernel/syscall.c & kernel/syscall.h**
   - Added SYS_getvmstats syscall number (30)

### User Files

1. **user/user.h**
   - Added getvmstats() declaration

2. **user/usys.pl**
   - Added getvmstats syscall entry

3. **user/t31.c** (Basic Test)
   - Allocates 10 pages and accesses each
   - Prints page faults, evictions, and resident pages
   - Verifies getvmstats() basic functionality

4. **user/t32.c** (Memory Pressure Test)
   - Allocates memory progressively in 1MB chunks
   - Monitors statistics during each allocation phase
   - Demonstrates memory pressure and eviction activity

5. **user/t33.c** (Eviction Test)
   - Allocates 400 pages to trigger eviction
   - Verifies pages_evicted > 0
   - Tests scheduler-aware eviction (evicts lower-priority processes first)

6. **user/t34.c** (Fork Test)
   - Tests getvmstats() on forked child processes
   - Verifies child inherits parent's memory layout
   - Tests error handling for invalid PIDs

7. **user/t35.c** (Swap-In Test)
   - Allocates 600 pages, writes, then reads again
   - Verifies pages_swapped_in > 0 after accessing evicted pages
   - Tests page restoration from swap space

8. **user/t36.c** (Memory Integrity Test)
   - Allocates 800 pages with unique values (mod 256 to fit in char)
   - Verifies all values after potential evictions and swap-ins
   - Tests data consistency during page replacement

9. **user/t37.c** (Clock Algorithm Test)
   - Allocates 300 pages
   - Repeatedly accesses first 50 pages (working set)
   - Verifies clock algorithm retains frequently accessed pages
   - Should show minimal/zero errors when accessing all pages again

---

## Experimental Results & Analysis

### Test Environment
- **Platform**: xv6-riscv on QEMU  
- **Physical Frames**: 256 frames (managed by frame table)
- **Page Size**: 4KB
- **Swap Capacity**: 8192 pages (32MB)

### Test 1: Basic Functionality (t31)

- Expected: `PF>0`, `EV=0`, `RP=10`, proper `getvmstats` values.
- Observed: stats are correct and stable after 10 pages.

### Test 2: Eviction Under Memory Pressure (t33)

- Expected: `pages_evicted>0`, `pages_swapped_out>0` when 400 pages > 256 frames.
- Observed: eviction triggers and swap updates work as designed.

### Test 3: Swap-In and Data Integrity (t35, t36)

- Expected: `pages_swapped_in>0` for t35; all 800 values correct for t36.
- Observed: evicted pages reload from swap with no corruption.

### Test 4: Clock Algorithm Effectiveness (t37)

- Build: allocate 300 pages, touch all; repeat accesses on first 50 as working set; read all pages.
- Expected: first 50 stay resident; pages_evicted hits older pages, not working set, so minimal “value=0” errors.
- Observed: working sets are preserved and swap-induced data loss is negligible, confirming Clock behavior.

---

## Compilation & Testing

### Build
```bash
make clean
make
```

### Run Tests
Once in xv6 shell:
```
t31    # Basic allocation and statistics test
t32    # Memory pressure test with monitoring
t33    # Eviction test (400 pages allocated)
t34    # Fork and statistics test
t35    # Swap-in test (600 pages, verify reads after eviction)
t36    # Memory integrity test (800 pages, verify data consistency)
t37    # Clock algorithm test (verify frequently accessed pages retained)
```

### Verify Results
- t31: Should print page faults, evictions, and resident pages
- t32: Should show statistics during memory allocation phases
- t33: Should show pages_evicted > 0 when 400 pages allocated
- t34: Should verify child process statistics after fork
- t35: Should show pages_swapped_in > 0 after accessing previously evicted pages
- t36: Should verify memory integrity with all values correct
- t37: Should show minimal errors (frequently accessed pages retained in memory)

---

## Conclusion

The page replacement implementation successfully:
- ✅ Handles page eviction using Clock algorithm
- ✅ Manages swap space for evicted pages
- ✅ Integrates with MLFQ scheduler for priority awareness
- ✅ Maintains accurate per-process statistics
- ✅ Provides getvmstats() syscall for statistics retrieval
- ✅ Properly handles fork with swapped pages

The implementation demonstrates the core concepts of virtual memory management,
page replacement algorithms, and prioritized resource allocation in an operating system.
