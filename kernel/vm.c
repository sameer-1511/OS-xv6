#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "buf.h"


//frame table
struct spinlock framelock;
struct frame frametable[MAXFRAMES];
int clock_hand = 0;

//swap space
// #define MAX_SWAP 8192
// #define PTE_S (1L << 9)

// char swapspace[MAX_SWAP][PGSIZE];
// int swap_used[MAX_SWAP];
// struct spinlock swaplock;

#define PTE_S (1L << 9)

// Disk-backed swap
// #define SWAP_START_BLOCK 1000
// #define MAX_SWAP_BLOCKS 8192
// #define BLOCKS_PER_PAGE (PGSIZE / BSIZE)

int swap_bitmap[MAX_SWAP_BLOCKS];
struct spinlock swaplock;

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Initialize the kernel_pagetable, shared by all CPUs.
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
  initlock(&framelock, "frametable");

  acquire(&framelock);
  for(int i = 0; i < MAXFRAMES; i++) {
    frametable[i].in_use = 0;
    frametable[i].owner = 0;
    frametable[i].va = 0;
    frametable[i].ref_bits = 0;
  }
  release(&framelock);

  // initlock(&swaplock, "swap");

  // for(int i = 0; i < MAX_SWAP; i++){
  //   swap_used[i] = 0;
  // }

  initlock(&swaplock, "swap");
  
  for(int i = 0; i < MAX_SWAP_BLOCKS; i++){
    swap_bitmap[i] = 0;
  }
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);

  if((*pte & PTE_V)){
    update_refbit(myproc(), PGROUNDDOWN(va));
  }
  return pa;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - PGSIZE;
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. It's OK if the mappings don't exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;   
    if((*pte & PTE_V) == 0){
      *pte = 0;
      continue;
    }

    if(do_free){
      uint64 pa = PTE2PA(*pte);
      // remove from frame table
      acquire(&framelock);
      for(int i = 0; i < MAXFRAMES; i++){
        if(frametable[i].in_use && frametable[i].owner != 0){
          pte_t *fp = walk(frametable[i].owner->pagetable, frametable[i].va, 0);
          if(fp && (*fp & PTE_V) && PTE2PA(*fp) == pa){
            frametable[i].in_use = 0;
            frametable[i].owner = 0;
            frametable[i].va = 0;
            frametable[i].ref_bits = 0;
            break;
          }
        }
      }
      release(&framelock);

      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// Allocate PTEs and physical memory to grow a process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory. Handles both in-memory
// pages and swapped-out pages.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz, struct proc *parent, struct proc *child)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;
  // extern char swapspace[][PGSIZE];
  // extern int swap_used[];
  extern struct spinlock swaplock;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;   // page table entry hasn't been allocated
    
    // Handle swapped pages
    if((*pte & PTE_S)) {
      // This is a swapped page - copy from swap space
      // int parent_slot = parent->swap_index[i/PGSIZE];
      // if(parent_slot < 0 || parent_slot >= MAX_SWAP) {
      //   goto err;  // Invalid swap slot
      // }

      int parent_block = parent->swap_index[i/PGSIZE];
      int child_block = allocate_swap_block();

      char *temp = kalloc();
      if(temp == 0) goto err;
      
      // Allocate new swap slot in swap space
      // acquire(&swaplock);
      // int child_slot = -1;
      // for(int j = 0; j < MAX_SWAP; j++){
      //   if(swap_used[j] == 0){
      //     swap_used[j] = 1;
      //     child_slot = j;
      //     break;
      //   }
      // }
      // release(&swaplock);
      // read parent data
      for(int k = 0; k < BLOCKS_PER_PAGE; k++){
        struct buf *b = bread(ROOTDEV, parent_block + k);
        memmove(temp + k*BSIZE, b->data, BSIZE);
        brelse(b);
      }

      // write to child block
      for(int k = 0; k < BLOCKS_PER_PAGE; k++){
        struct buf *b = bread(ROOTDEV, child_block + k);
        memmove(b->data, temp + k*BSIZE, BSIZE);
        bwrite(b);
        brelse(b); 
      }

      kfree(temp);
      
      // if(child_slot < 0) {
      //   goto err;  // No swap space available
      // }
      
      // Copy data from parent's swap slot to child's
      // memmove(swapspace[child_slot], swapspace[parent_slot], PGSIZE);
      
      // Get the pointer to PTE in new page table
      pte_t *newpte = walk(new, i, 1);
      if(newpte == 0)
        goto err;
      
      // Copy the swapped PTE with PTE_S flag
      *newpte = (*pte) | PTE_S;  // Copy PTE flags, set swapped bit
      *newpte &= ~PTE_V;  // Ensure valid bit is not set
      *newpte |= PTE_S;   // Ensure swapped bit is set
      
      child->swap_index[i/PGSIZE] = child_block;
      continue;  // Skip to next page
    }
    
    if((*pte & PTE_V) == 0)
      continue;   // physical page hasn't been allocated
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
    child->resident_pages++;
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
  
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    pte = walk(pagetable, va0, 0);
    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;
      
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    update_refbit(myproc(), va0);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);

    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);
    update_refbit(myproc(), va0);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0){
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);

  p->page_faults++;

  pte_t *pte = walk(pagetable, va, 0);
  // if(pte){
  //   printf("FAULT: va=%lx pte=%p flags=%lx\n", va, pte, *pte);
  // }

  //Swapped page
  if(pte && (*pte & PTE_S)) {
    // Enforce frame-limit-based eviction, even if kalloc() still has physical pages.
    //printf("Swap In triggered for va=0x%lx\n", va);
    if(!free_frame_exists()) {
      struct frame *victim = select_eviction_frame();
      evict_page(victim);
    }

    mem = (uint64) kalloc();
    if(mem == 0){
      struct frame *victim = select_eviction_frame();    
      evict_page(victim);
      mem = (uint64) kalloc();
      if(mem == 0){
        panic("vmfault: out of memory after eviction");
      }
    }

    // int slot = p->swap_index[va/PGSIZE];
    // memmove((void*)mem, swapspace[slot], PGSIZE);

    // p->swap_index[va/PGSIZE] = -1;

    // acquire(&swaplock);
    // swap_used[slot] = 0;
    // release(&swaplock);
    int block = p->swap_index[va/PGSIZE];

    for(int i = 0; i < BLOCKS_PER_PAGE; i++){
      struct buf *b = bread(ROOTDEV, block + i);
      memmove((void*)(mem + i * BSIZE), b->data, BSIZE);
      brelse(b);
    }
    // free swap block  
    acquire(&swaplock);
    int idx = (block - SWAP_START_BLOCK) / BLOCKS_PER_PAGE;
    if(idx >= 0 && idx < MAX_SWAP_BLOCKS){
      swap_bitmap[idx] = 0;
    }
    release(&swaplock);

    p->swap_index[va/PGSIZE] = -1;

    *pte = PA2PTE(mem) | PTE_R | PTE_W | PTE_U | PTE_V;
    *pte &= ~PTE_S; 

    sfence_vma(); //flush TLB

    acquire(&framelock);
    for(int i = 0; i<MAXFRAMES; i++) {
      if(frametable[i].in_use == 0) {
        frametable[i].in_use = 1;
        frametable[i].owner = p;
        frametable[i].va = va;
        frametable[i].ref_bits = 1;
        break;
      }
    }
    release(&framelock);

    p->pages_swapped_in++;
    p->resident_pages++;
    return mem;
  }
  
  //Normal page
  if(ismapped(pagetable, va)) {
    return 0;
  }

  // Enforce frame-limit-based eviction, even if kalloc() has free pages.
  if(!free_frame_exists()) {
    struct frame *victim = select_eviction_frame();
    evict_page(victim);
  }

  mem = (uint64) kalloc();
  if(mem == 0){
    struct frame *victim = select_eviction_frame();
    evict_page(victim);
    mem = (uint64) kalloc();
    if (mem == 0){
      panic("vmfault: out of memory after eviction");
    }
  }

  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }

  acquire(&framelock);
  for(int i = 0; i < MAXFRAMES; i++) {
    if(frametable[i].in_use == 0) {
      frametable[i].in_use = 1;
      frametable[i].owner = p;
      frametable[i].va = va;
      frametable[i].ref_bits = 1;
      break;
    }
  }
  release(&framelock);

  sfence_vma(); //flush TLB

  p->resident_pages++;
  return mem;
}

int
ismapped(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0) {
    return 0;
  }
  // if (*pte & PTE_V || *pte & PTE_S) {
  if(*pte & PTE_V) {
    return 1;
  }
  return 0;
}

// Return 1 if there is at least one free frame in the frame table.
int
free_frame_exists(void)
{
  int exists = 0;
  acquire(&framelock);
  for(int i = 0; i < MAXFRAMES; i++) {
    if(frametable[i].in_use == 0) {
      exists = 1;
      break;
    }
  }
  release(&framelock);
  return exists;
}

//Eviction
struct frame* select_eviction_frame(){

  acquire(&framelock);
  struct frame *best = 0;
  int count = 0;

  while(count < 2*MAXFRAMES){
    struct frame* f = &frametable[clock_hand];
    clock_hand = (clock_hand + 1) % MAXFRAMES;
    count++;

    if(!f->in_use || f->owner == 0){
      continue;
    }
    pte_t *pte = walk(f->owner->pagetable, f->va, 0);
    if(pte == 0 || (*pte & PTE_V) == 0){
      continue;
    }
    if(f->ref_bits == 1){
      f->ref_bits = 0;
      continue;
    }

    if(best == 0 || f->owner->qlevel > best->owner->qlevel){
      best = f;
    }
  }

  if(best == 0){
    release(&framelock);
    panic("select_eviction_frame: no frame to evict");
  }

  best->in_use = 2; // Mark as in use to prevent eviction by another thread
  release(&framelock);
  return best;
}
// struct frame* select_eviction_frame(){
//   struct frame *best = 0;

//   acquire(&framelock);

//   for(int count = 0; count < MAXFRAMES; count++){
//     struct frame* f = &frametable[clock_hand];

//     if(f->in_use && f->owner != 0){

//       // NEW: validate mapping
//       pte_t *pte = walk(f->owner->pagetable, f->va, 0);
//       if(pte == 0 || (*pte & PTE_V) == 0){
//         clock_hand = (clock_hand + 1) % MAXFRAMES;
//         continue;
//       }

//       if(f->ref_bits == 0){
//         if(best == 0 || f->owner->qlevel > best->owner->qlevel){
//           best = f;
//         }
//       } else {
//         f->ref_bits = 0;
//       }
//     }

//     clock_hand = (clock_hand + 1) % MAXFRAMES;
//   }

//   release(&framelock);

//   if(best == 0){
//     panic("select_eviction_frame: no frame");
//   }

//   return best;
// }

void evict_page(struct frame *victim){
  struct proc *p = victim->owner;
  uint64 va = victim->va;

  pte_t *pte = walk(p->pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0){
    return; // Page is not mapped, nothing to evict
  }
  uint64 pa = PTE2PA(*pte);

  // acquire(&swaplock);
  // int slot = -1;
  // for(int i = 0; i < MAX_SWAP; i++){
  //   if(swap_used[i] == 0){
  //     swap_used[i] = 1;
  //     slot = i;
  //     break;
  //   }
  // }
  // release(&swaplock);

  // if(slot == -1)
  //   panic("swap full");

  // memmove(swapspace[slot], (void*)pa, PGSIZE);

  // if(va / PGSIZE >= MAX_PROC_PAGES)
  //   panic("evict_page: va out of swap_index range");
  // p->swap_index[va/PGSIZE] = slot;

  int block = allocate_swap_block();

  for(int i = 0; i < BLOCKS_PER_PAGE; i++){
    struct buf *b = bread(ROOTDEV, block + i);
    memmove(b->data, (void*)(pa + i * BSIZE), BSIZE);
    bwrite(b);
    brelse(b);
  }
  
  p->swap_index[va/PGSIZE] = block;

  *pte &= ~PTE_V;   // invalidate
  *pte |= PTE_S;    // mark swapped
  //printf("EVICT: va=%lx pte=%p flags=%lx\n", va, pte, *pte);

  sfence_vma(); //flush TLB

  kfree((void*)pa);

  p->pages_evicted++;
  p->pages_swapped_out++;
  p->resident_pages--;

  acquire(&framelock);
  victim->in_use = 0;
  victim->owner = 0;
  victim->va = 0;
  victim->ref_bits = 0;
  release(&framelock);
}

void update_refbit(struct proc *p, uint64 va) {
  acquire(&framelock);

  for(int i = 0; i < MAXFRAMES; i++) {
    if(frametable[i].in_use &&
       frametable[i].owner == p &&
       frametable[i].va == va) {
      frametable[i].ref_bits = 1;
      break;
    }
  }

  release(&framelock);
}

int allocate_swap_block() {
  acquire(&swaplock);
  for(int i = 0; i < MAX_SWAP_BLOCKS; i++){
    if(swap_bitmap[i] == 0){
      swap_bitmap[i] = 1;
      release(&swaplock);
      return SWAP_START_BLOCK + i * BLOCKS_PER_PAGE;
    }
  }
  release(&swaplock);
  panic("No swap space left on disk");
}