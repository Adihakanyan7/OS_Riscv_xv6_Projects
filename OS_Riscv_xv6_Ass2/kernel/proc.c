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

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->p_lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++) {
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      uint64 va = KSTACK((int) ((p - proc) * NKT + (kt - p->kthread)));
      kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
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
      initlock(&p->p_lock, "proc");
      p->p_state = UNUSED;
      kthreadinit(p);
  }
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
  struct proc *p = 0;
  if(c->thrd != 0){
    p = c->thrd->pcb;
  }
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
// and return with p->p_lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->p_lock);
    if(p->p_state == UNUSED) {
      goto found;
    } else {
      release(&p->p_lock);
    }
  }
  return 0;

found:
  p->p_state = USED;
  
  p->pid = allocpid();
  
  
  // Allocate a trapframe page.
  if((p->base_trapframes = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->p_lock);
    return 0;
  }
  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->p_lock);
    return 0;
  }  
  
  p->counter = 1;

  allocKthread(p);
  
  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->p_lock must be held.
static void
freeproc(struct proc *p)
{
  
  if(p->base_trapframes)
    kfree((void*)p->base_trapframes);
  p->base_trapframes = 0;
  
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  struct kthread *kt;
  
  for(kt = p->kthread; kt < &p->kthread[NKT];kt++){
    acquire(&kt->t_lock);
    freeKthread(kt);
    release(&kt->t_lock);
  }
  
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->p_killed = 0;
  p->p_xstate = 0;
  p->p_state = UNUSED;
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
  if(mappages(pagetable, TRAPFRAME(0), PGSIZE,
              (uint64)(p->base_trapframes), PTE_R | PTE_W) < 0){
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
  uvmunmap(pagetable, TRAPFRAME(0), 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->kthread[0].trapframe->epc = 0;      // user program counter
  p->kthread[0].trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  //p->p_state = USED;

  p->kthread[0].t_state = RUNNABLE;

  release(&p->kthread[0].t_lock);

  release(&p->p_lock);
}

// Grow or shrink user memory by n bytes.
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

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  struct kthread *kt = mykthread();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    //freeKthread(&np->kthread[0]);
    release(&np->p_lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->kthread[0].trapframe) = *(kt->trapframe);

  // Cause fork to return 0 in the child.
  np->kthread[0].trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->kthread[0].t_lock);
  release(&np->p_lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->p_lock);
  acquire(&np->kthread[0].t_lock);
  np->kthread[0].t_state = RUNNABLE;
  //np->p_state = USED; ////////////////////////////////////////////////////////////////////////////
  release(&np->kthread[0].t_lock);
  release(&np->p_lock);

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
exit(int status)
{
  struct proc *p = myproc();
  struct kthread *t = mykthread();

  for(struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    if(t != kt){
       int changer = 1;
      kthread_join(kt->tid, (uint64)&changer);
    }
  }
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
  
  acquire(&p->p_lock);

  p->p_xstate = status;
  
  p->p_state = ZOMBIE;

  // for(struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++){
  //   acquire(&kt->t_lock);
  //   freeKthread(kt);
  //   if(kt != mykthread())
  //     release(&kt->t_lock);
  // }

  // struct kthread *kt = mykthread();
  // acquire(&kt->t_lock);
  // kt->t_state = ZOMBIE;
  // kt->t_xstate = status;
  
  release(&p->p_lock);
  release(&wait_lock);
  

  acquire(&t->t_lock);
  
  freeKthread(t);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
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
        acquire(&pp->p_lock);

        havekids = 1;
        if(pp->p_state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->p_xstate,
                                  sizeof(pp->p_xstate)) < 0) {
            release(&pp->p_lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->p_lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->p_lock);
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
  struct proc* p;
  struct cpu *c = mycpu();

  c->thrd = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->p_lock);
      if(p->p_state == USED) {
        release(&p->p_lock);
        struct kthread *kt;
        for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
          acquire(&kt->t_lock);

          if(kt->t_state == RUNNABLE){
              kt->t_state = RUNNING;
              c->thrd = kt;
              swtch(&c->context, &kt->context);
              c->thrd = 0;
          }

          release(&kt->t_lock);

        }
      }
      else{
        release(&p->p_lock);
      }
    }
  }
}

// Switch to scheduler.  Must hold only p->p_lock
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
  struct kthread *kt = mykthread();

  if(!holding(&kt->t_lock))
    panic("sched p->p_lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(kt->t_state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&kt->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void) 
{
  acquire(&mykthread()->t_lock);
  
  mykthread()->t_state = RUNNABLE;

  sched();
  
  release(&mykthread()->t_lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->p_lock from scheduler.
  release(&mykthread()->t_lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct kthread *kt = mykthread();
  //struct proc *p = myproc();
  
  // Must acquire p->p_lock in order to
  // change p->p_state and then call sched.
  // Once we hold p->p_lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->p_lock),
  // so it's okay to release lk.

  acquire(&kt->t_lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  kt->chan = chan;
  kt->t_state = SLEEPING;

  sched();

  // Tidy up.
  kt->chan = 0;

  // Reacquire original lock.
  release(&kt->t_lock);
  acquire(lk);
}

// Wake up all Kthreads sleeping on chan.
// Must be called without any p->p_lock.
void
wakeup(void *chan) 
{
  struct kthread *kt;
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->p_lock);

      if(p->p_state == USED){
        release(&p->p_lock);

        for(kt = p->kthread;kt < &p->kthread[NKT]; kt++){
        struct kthread *my_kt = mykthread();
        if(kt != my_kt){
        acquire(&kt->t_lock);

        if(kt->t_state == SLEEPING) {
          if(kt->chan == chan){
            kt->t_state = RUNNABLE;
          }
        }

        release(&kt->t_lock);
        }
      }

      }

      else{
      release(&p->p_lock);
      }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid) 
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->p_lock);
    if(p->pid == pid){
      p->p_killed = 1;
      struct kthread *kt;
      for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
        struct kthread *my_kt = mykthread();
        if(kt != my_kt){
        acquire(&kt->t_lock);
        if(kt->t_state == SLEEPING){
        // Wake Kthread from sleep().
        kt->t_state = RUNNABLE;
        }
        release(&kt->t_lock);
        }else{
          continue;
        }
      }
      release(&p->p_lock);
      return 0;
    }
    release(&p->p_lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->p_lock);
  p->p_killed = 1;
  release(&p->p_lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->p_lock);
  k = p->p_killed;
  release(&p->p_lock);
  return k;
}

// int
// killedThread(struct kthread *kt)
// {
//   int k;
  
//   acquire(&kt->t_lock);
//   k = kt->t_killed;
//   release(&kt->t_lock);
//   return k;
// }

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
  //[SLEEPING]  "sleep ",
  //[RUNNABLE]  "runble",
  //[RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->p_state == UNUSED)
      continue;
    if(p->p_state >= 0 && p->p_state < NELEM(states) && states[p->p_state])
      state = states[p->p_state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

int
kthread_create( uint64 start_func, uint64 stack, uint stack_size ){
  struct proc *p = myproc();
  struct kthread *kt = allocKthread(p);
  if(kt){
  struct kthread *my_kt = mykthread();
  *(kt->trapframe) =*(my_kt->trapframe);

  kt->t_state = RUNNABLE;

  kt->trapframe->sp = stack + stack_size;
  kt->trapframe->epc = start_func;
  
  release(&kt->t_lock);
  
  return kt->tid; 
  }
  
  return -1;
}

int kthread_id(){
  return mykthread()->tid;
}

int kthread_kill(int ktid){ 
  struct proc *p = myproc();
  struct kthread *kt;
  for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->t_lock);
    if(kt->tid == ktid){
      if(kt->t_state == SLEEPING){
        kt->t_state = RUNNABLE;
      }
      kt->t_killed = 1;
      release(&kt->t_lock);
      return 0;
    }
    release(&kt->t_lock);
  }
  return -1;
}

void
kthread_exit(int status){ 
  //struct kthread *kt = mykthread();
  struct kthread *k;
  int num_of_rel_thrds = 0;
  
  acquire(&myproc()->p_lock);

  for(k = myproc()->kthread; k <&myproc()->kthread[NKT];k++){
    acquire(&k->t_lock);
    if(k->t_state == Kthread_UNUSED){
      release(&k->t_lock);
      continue;
    }

    if(k->t_state == Kthread_ZOMBIE){
      release(&k->t_lock);
      continue;
    }
    
    num_of_rel_thrds += 1;
    release(&k->t_lock);
  }

  release(&myproc()->p_lock);

  if(num_of_rel_thrds != 1){
    acquire(&mykthread()->t_lock);

    mykthread()->t_state = Kthread_ZOMBIE;
    mykthread()->t_xstate = status;
    
    release(&mykthread()->t_lock);

    wakeup(mykthread());
    
    acquire(&mykthread()->t_lock);
    
    sched();
  }
  else{
    exit(status);
  }
}

int
kthread_join(int ktid, uint64 status){
  struct kthread *kt;
  struct kthread *tt = 0;
  struct proc *p = myproc();
  
  acquire(&p->p_lock);
  for(;;){
    int counter = 0;
    for(kt = p->kthread; kt < &p->kthread[NKT]; kt++){
      acquire(&kt->t_lock);
      if(kt->tid == ktid){
        if(kt->t_state != Kthread_UNUSED){
        counter = 1;
        tt = kt;
        if(kt->t_state == Kthread_ZOMBIE){
          if(status != 0 && copyout(p->pagetable, status, (char *)&kt->t_xstate,
                                  sizeof(kt->t_xstate)) < 0) {
            release(&kt->t_lock);
            release(&p->p_lock);
            return -1;
          }
          freeKthread(kt);
          release(&kt->t_lock);
          release(&p->p_lock);
          return 0;
        }
      }
      }
      release(&kt->t_lock);
    }

    struct kthread *my_kt = mykthread();

    if(!counter || my_kt->t_killed == 1){
      release(&p->p_lock);
      return -1;
    }

    sleep(tt, &p->p_lock);  
  }
}