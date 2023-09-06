#include "kernel/types.h"

#define STACK_SIZE  4000
#define MAX_UTHREADS  4
#define NULL 0

enum sched_priority { LOW, MEDIUM, HIGH };

/* Possible states of a thread: */
enum tstate { FREE, RUNNING, RUNNABLE };

// Saved registers for context switches.
struct context {
    uint64 ra;
    uint64 sp;

    // callee saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

struct uthread {
    char                ustack[STACK_SIZE];  // the thread's stack
    enum tstate         state;          // FREE, RUNNING, RUNNABLE
    struct context      context;        // uswtch() here to run process
    enum sched_priority priority;       // scheduling priority
};

extern void uswtch(struct context*, struct context*);

int uthread_create(void (*start_func)(), enum sched_priority priority);

void uthread_yield();
void uthread_exit();

int uthread_start_all();
enum sched_priority uthread_set_priority(enum sched_priority priority);
enum sched_priority uthread_get_priority();

struct uthread* uthread_self();

struct uthread* high_pr_thread();