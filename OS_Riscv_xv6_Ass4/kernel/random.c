#include <stdarg.h>
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

int seed = 0x2A;


struct spinlock lock;
 


int
write(int fd, const void *src, int n)
{
    if(n!=1){
        return -1;
    }
    acquire(&lock);
    if(either_copyin(&seed, fd, src, 1) == -1){
        release(&lock);
        return -1;
    }
    release(&lock);
    return 1;
}

int
read(int fd, void *dst, int n)
{
    int count = 0;
  for(int i=0;i<n;i++){
    acquire(&lock);
    if(either_copyout(&seed,fd,dst, 1) == -1){
        release(&lock);
        break;
    }
    count++;
    release(&lock);
  }
  return count;
}

void
randominit(void)
{
  initlock(&lock, "random");
  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[RANDOM].read = read;
  devsw[RANDOM].write = write;
}