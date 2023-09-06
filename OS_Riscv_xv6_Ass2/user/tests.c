#include "kernel/types.h"
#include "user.h"
#include "uthread.h"

void high(){
    printf("high priority\n");
    uthread_exit();
}

void medium(){
    printf("medium priority\n");
    uthread_exit();
}

void another_high(){
    printf("another high priority\n");
    uthread_exit();
}

void low(){
    printf("low priority\n");
    uthread_exit();
}

int main(int argc, char *argv[])
{
   if(uthread_create(high, HIGH) < 0){
       printf("error high");
    }
    if(uthread_create(another_high,HIGH) < 0){
        printf("error high");
    }
    if(uthread_create(low,LOW) < 0){
        printf("error low");
    }
    if(uthread_create(medium,MEDIUM) < 0){
        printf("error mediom");
    }

    uthread_start_all();
    return 0;
}