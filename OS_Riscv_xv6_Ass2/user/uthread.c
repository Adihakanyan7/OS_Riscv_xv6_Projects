#include "kernel/types.h"
#include "uthread.h"
#include "user.h"

struct uthread threadArray[MAX_UTHREADS+1];

struct uthread *currThread = &threadArray[MAX_UTHREADS];

int init = 1; // True


int
uthread_create(void (*start_func)(), enum sched_priority priority){
    struct uthread *t;
    for(t = threadArray;t < &threadArray[MAX_UTHREADS];t++){
        if(t->state == FREE){
            t->priority = priority;
            t->context.sp = (uint64)t->ustack + STACK_SIZE - sizeof(uint64);
            t->context.ra = (uint64)start_func;
            t->state = RUNNABLE;
            if(currThread == NULL){
                currThread = t;
            }
            return 0;            
        }
    }
    return -1;
}

void
uthread_yield(){
    struct uthread *t;
    struct uthread *temp = 0;
    enum sched_priority curr_priority = LOW;

    for(t = threadArray;t < &threadArray[MAX_UTHREADS];t++){
        if(t->state == RUNNABLE){
            if(t->priority >= curr_priority){
                temp = t;
                curr_priority = t->priority;  
            }  
        }
    }
    if(!temp){
        return;
    }else{
        if(temp != currThread){
            temp->state = RUNNING;
            struct uthread *prev = currThread;
            currThread = temp;
            uswtch(&(prev->context),&(temp->context));
        }
    }
}

void
uthread_exit(){
    currThread->state = FREE;

    struct uthread *t;

    int shouldTerminate = 1; // True

    for(t = threadArray;t < &threadArray[MAX_UTHREADS];t++){
        if(currThread != t){
                if(t->state == RUNNABLE || t->state == RUNNING){
                    shouldTerminate = 0;
                    break;
                }
        }
    }

    if(shouldTerminate){
        exit(0);
    }
    uthread_yield();
}

enum sched_priority
uthread_set_priority(enum sched_priority priority){
    enum sched_priority ans = currThread->priority;
    currThread->priority = priority;
    return ans;
}

enum sched_priority
uthread_get_priority(){
    return currThread->priority;
}

int
uthread_start_all(){
    if(!init)
        return -1;
    else{
        init = 0;
        uthread_yield();
        while (1){} 
    }
}

struct uthread*
uthread_self(){
    return currThread;
}






