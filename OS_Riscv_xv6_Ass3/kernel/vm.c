#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

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
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // map kernel stacks
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
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
  return pa;
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

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
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

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  struct proc *p = myproc();
  struct Pdata *page;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if(((*pte & PTE_V) == 0) && ((*pte & PTE_PG) == 0))
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if((*pte & PTE_V) && do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    
    // here we search for address a, and if it exists in memory, we delete it 
    if(p->pid >= 3){
      if(pagetable == p->pagetable && do_free && (*pte & PTE_V)){
        for(page=p->pagesInMem;page < &p->pagesInMem[MAX_PSYC_PAGES];page++){
          if(page->va == a){
            page->va = 0;

            page->flag = 0;
            page->level = 0;

            p->numOfPagesInMem = p->numOfPagesInMem-1;

            break;
          }
        }
      }
    }

    // if we got here, this means we have to remove it from the swap file
    if(p->pid >= 3){
        if(pagetable==p->pagetable&&(*pte&PTE_PG)){
        for(page=p->pagesInSwapfile;page<&p->pagesInSwapfile[MAX_PSYC_PAGES];page++){
          if(page->va == a){
            p->numOfPagesInSwapfile--;

            page->va = 0;

            page->flag = 0;
            page->level = 0;
            
            break;
          }
        }
      }
    } 

    *pte = 0;
  }
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

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("uvmfirst: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  struct proc *p = myproc();

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

    //here we add a new page to the proc pages 
    if (p->pid >= 3)
      pMemUpdater(a,pagetable); 
  
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
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0 && (*pte & PTE_PG) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
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

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

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
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

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
    if(pa0 == 0)
      return -1;
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

int getPage(){
  // it all depends on the chosen algorithm (Task 3)

  #ifdef LAPA
  struct proc *p = myproc();
  struct Pdata *page;

  int min = 64;
  int minIndex = -1;

  for(page = p->pagesInMem+1;page < &p->pagesInMem[MAX_PSYC_PAGES];page++){
    int count=0;
    int loop=0;
    if(page->flag){
        for(int i=0;i<64 && !stoploop;i++){
          // this masks the 64 bits 
          uint64 mask = 1 << i;

          if((page->level&mask)!=0)
              // this means we found one
              count++;
          if(count>min)
            // if we got here, there is a loop
            loop=1;
        }

        if(count<min || (minIndex ==-1 && count<=min)){
          minIndex=(int)(page-p->pagesInMem);
          min=count;
        }
      }
    }

    return minIndex;
  #endif

  #ifdef SCFIFO
  struct proc *p = myproc();
  struct Pdata *page;

  loop:
  time = (uint64)~0;
  minIndex = 1;

  for(page=p->pagesInMem;page<&p->pagesInMem[MAX_PSYC_PAGES];page++){ 
    if(page->level <= time && page->flag){
      minIndex=(int)(page-p->pagesInMem);
      time = page->level;
    }
  }

  pte_t* pte = walk(p->pagetable, p->pagesInMem[minIndex].va, 0);

  // this means we should give a second chance
  if((*pte & PTE_A)!=0){  
    *pte &=~ PTE_A;
    p.timerForPg = p.timerForPg + 1;
    p->pagesInMem[minIndex].level = p->timerForPg; 

    // search once more
    goto loop;  
  }

  return minIndex;

  #endif

  #ifdef NFUA

  uint64 age = ~0;
  struct proc *p = myproc();
  struct Pdata *page;
  int minIndex = 1;

  for(page=p->pagesInMem+1;page<&p->pagesInMem[MAX_PSYC_PAGES];page++){
    if(page->flag&&page->level<age){
      minIndex = (int)(page - p->pagesInMem);
      age = page->level;
    }
  }

  return minIndex;

  #endif
  //
  return 1;
}

int findIndexMem(){
  struct proc *p = myproc();
  struct Pdata *page;

  for(page=p->pagesInMem;page<&p->pagesInMem[MAX_PSYC_PAGES];page++)
    if(page->flag == 0)
      return (int)(page-p->pagesInMem);

  return -1;
}

// this is a helper function that handles page fault from the trap.c file
int pageFaulter(){
  struct proc *p = myproc();

  // trap value supervisor
  uint64 virt = r_stval();
  uint64 sw = PGROUNDDOWN(virt);
  //
  pte_t* pte = walk(p->pagetable, r_stval(), 0);

  // this means a segmentation fault
  if(!(*pte & PTE_PG))
    return 0;

  else {   //if swapped out, bring back from memory

    struct Pdata *page;

    // if memory is full then swap out a page
    if(p->numOfPagesInMem == MAX_PSYC_PAGES)
      swapper(p->pagetable);

    // place page from swapFile into memory
    char *mem = kalloc();

    // now we try to find the swapped file and change it accordingly
    for(page=p->pagesInSwapfile;page<&p->pagesInSwapfile[MAX_PSYC_PAGES];page++){

      //this means we found it
      if(sw == page->va){

        readFromSwapFile(p, mem,(page-p->pagesInSwapfile)*PGSIZE, PGSIZE);
        
        int minIndx = findIndexMem();
        struct Pdata *pge = &p->pagesInMem[minIndx];

        pge->va = page->va;
        pge->flag = 1;
        
        // now we change the fields according to the chosen algorithm
        #ifdef SCFIFO
        p->timerForPg = p->timerForPg + 1;
        pge->level = p->timerForPg;
        #endif

        #ifdef NUFA
        pge->level = 0;    
        #endif

        #ifdef LAPA
        pge->level = (uint64)~0;
        #endif

        // reset the page in swapfile
        page->va = 0;
        page->level = 0;
        page->flag = 0;

        // increment swapfile field
        p->numOfPagesInMem = p->numOfPagesInMem + 1;

        // decrement swapfile field
        p->numOfPagesInSwapfile = p->numOfPagesInSwapfile - 1;

        pte_t* g_pte = walk(p->pagetable, sw, 0);
        //
        *g_pte &= ~PTE_PG;
        *g_pte |= PTE_V;
        *g_pte = PA2PTE((uint64)mem) | PTE_FLAGS(*pte);
        
        break;
      }
    }

    // now we flush the TLB and return
    sfence_vma();
    //
    return 3;
  }
}

// this is a helper function that swaps a page from memory to the file, using on of the implemnted algorithms
void swapper(pagetable_t pagetable){
  struct proc *p = myproc();
  struct Pdata *page;

  if(p->numOfPagesInSwapfile+p->numOfPagesInMem==MAX_TOTAL_PAGES){ // error
    panic("there are more than 32 pages for the proccess");
  }
  
  struct Pdata *pSwap = &(p->pagesInMem[getPage()]); // we find the page according to the algoritm

  //now we need to find an empty place in swapped pages array and swap out page to it and write the page to swapfile.
  for(page = p->pagesInSwapfile;page<&p->pagesInSwapfile[MAX_PSYC_PAGES];page++){
    if(page->flag == 0){
      page->va = pSwap->va;

      // assign the flag bit
      page->flag = 1;
      
      pte_t* pte = walk(pagetable, page->va, 0);

      uint64 pa = PTE2PA(*pte);

      writeToSwapFile(p, (char*)pa, (page - p->pagesInSwapfile)*PGSIZE, PGSIZE);

      // since we wrote to swapfile, we increment the proccess's field accordingly
      p->numOfPagesInSwapfile = p->numOfPagesInSwapfile + 1;

      // 
      kfree((void*)pa); 
      
      // we reset the swapped page
      pSwap->va = 0;
      pSwap->flag = 0;
      
      p->numOfPagesInMem = p->numOfPagesInMem - 1;

      // here we check if  it  out to secondary storage
      *pte |= PTE_PG; 

      // the page is now in the paging file, so the process's PTE should know that the page is not there anymore
      // hence, we reset the valid bit, AKA the flag 
      *pte &= ~PTE_V;     
      
      // here we flush the TLB
      sfence_vma(); 
      //
      break;
    }
  }
}

// this is a helper function that adds a new page that was in the uvmalloc() function to the proccess's pages stack
void pMemUpdater(uint64 a, pagetable_t pagetable){
  struct proc *p = myproc();

  // if it is full - chose a page to swap out instead
  if(p->numOfPagesInMem == MAX_PSYC_PAGES)
    swapper(pagetable);

  // now we find the minimal index
  int indx = findIndexMem();

  // now we extract the required page with minimal index, and handle it
  struct Pdata *page = &p->pagesInMem[indx];

  page->va = a;
  page->flag = 1;
  
  // now we change fields, depending on the algorithm we are using
  #ifdef SCFIFO
  p->timerForPg = p->timerForPg + 1; 
  page->level= p->timerForPg;
  #endif
  //
  #ifdef LAPA
  page->level = (uint64)~0;
  #endif
  //
  #ifdef NFUA
  page->level =0;
  #endif
  
  // now we need to increment the field in the proccess
  p->numOfPagesInMem = p->numOfPagesInMem + 1;

  pte_t* pte = walk(pagetable, page->va, 0);

  // we reset the valid bit, AKA the flag 
  *pte |= PTE_V;
  
  // here we check if  it  out to secondary storage
  *pte &= ~PTE_PG;
  
}