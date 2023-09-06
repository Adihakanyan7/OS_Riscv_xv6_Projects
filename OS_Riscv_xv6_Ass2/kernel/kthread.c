#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

void kthreadinit(struct proc *p)
{
  initlock(&p->id_lock, "tid_lock");
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->t_lock, "thread");
    kt->t_state = Kthread_UNUSED;
    // kt->pcb = p;
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

struct kthread *mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->thrd;
  pop_off();
  return kt;
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}

int
allockid(struct proc *p)
{
  int kid;
  acquire(&p->id_lock);
  kid = p->counter;
  p->counter = p->counter + 1;
  release(&p->id_lock);

  return kid;
}

struct kthread*
allocKthread(struct proc *p)
{
  struct kthread *kt;
  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++){
    acquire(&kt->t_lock);
    if(kt->t_state == Kthread_UNUSED) {
      goto found;
    } else {
      release(&kt->t_lock);
    }
  }
  return 0;

found:
  kt->tid = allockid(p);
  kt->t_state = Kthread_USED;
  kt->pcb = p;
  kt->trapframe = get_kthread_trapframe(p, kt);
  memset(&kt->context, 0, sizeof(kt->context));
  kt->context.ra = (uint64)forkret;
  kt->context.sp = kt->kstack + PGSIZE;
  
  return kt;
}

void
freeKthread(struct kthread *kt)
{
  kt->trapframe = 0;
  kt->chan = 0;
  //memset(&kt->context, 0, sizeof(kt->context));
  //kt->kstack = 0;
  kt->pcb = 0;
  kt->t_killed = 0;
  kt->t_state = Kthread_UNUSED;
  kt->t_xstate = 0;
  kt->tid = 0;
}