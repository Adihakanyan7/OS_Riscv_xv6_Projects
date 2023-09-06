#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

#define PGSIZE 4096

//Test1 - Allocating 18 pages, some of them on disk, some in memory.
// then trying to access all arrays that was allocated.
void test1(){
    fprintf(2,"-------------test1 start ---------------------\n");
    int i;
    int j;
    int pid = fork();
    if(!pid){
        char* malloc_array[18];
        for(i = 0; i<18; i++){
           malloc_array[i] = sbrk(PGSIZE); 
           fprintf(2,"i = %d: allocated memory = %p\n", i, malloc_array[i]);
        }
        fprintf(2,"Allocated 18 pages, some of them on disk\n");
        fprintf(2,"Lets try to access all pages:\n");
        for(i = 0; i<18; i++){
            for(j = 0; j<PGSIZE; j++)
                malloc_array[i][j] = 'x'; 
            fprintf(2,"Filled array num=%d with chars\n", i);
        }
        exit(0);
    }else{
        int status;
        pid = wait(&status);
        fprintf(2,"child: pid = %d exit with status %d\n", pid, status);
    }
        fprintf(2,"-------------test1 finished ---------------------\n");

}

//Test2 testing alloc and dealloc (testing that delloa works fine, 
//and we dont recieve panic: more that 32 pages for process)
void test2(){
    fprintf(2,"-------------test2 start ---------------------\n");
    char* i;
    i = sbrk(20*PGSIZE);
    fprintf(2,"allocated memory = %p\n", i);
    i = sbrk(-20*PGSIZE);
    fprintf(2,"deallocated memory = %p\n", i);
    i = sbrk(20*PGSIZE);
    fprintf(2,"allocated memory = %p\n", i);
    i = sbrk(-20*PGSIZE);
    fprintf(2,"deallocated memory = %p\n", i);
    i = sbrk(20*PGSIZE);
    fprintf(2,"allocated memory = %p\n", i);
    i = sbrk(-20*PGSIZE);
    fprintf(2,"deallocated memory = %p\n", i);

    fprintf(2,"-------------test2 finished ---------------------\n");


}

//Test3 - parent allocates a lot of memory, forks, 
//and child can access all his data
void test3(){
    fprintf(2,"-------------test3 start ---------------------\n");
    uint64 i;
    char* arr = malloc(PGSIZE*17);
    for(i = 0; i < PGSIZE*17; i+=PGSIZE){
        arr[i] = 'a';
        fprintf(2,"dad: arr[%d]=%c\n", i, arr[i]);
    }
    int pid = fork();
    if(!pid){
        for(i=0; i < PGSIZE*17; i+=PGSIZE){
            fprintf(2,"child: arr[%d]=%c\n", i, arr[i]);
        }
        exit(i);
    }else{
        int status;
        pid = wait(&status);
        fprintf(2,"child: pid = %d exit with status %d\n", pid, status);
        sbrk(-17*PGSIZE);
    }
        fprintf(2,"-------------test3 finished ---------------------\n");

}




int main(int argc, char** argv){
    test1();
    test2();
    test3();

    exit(0);
}