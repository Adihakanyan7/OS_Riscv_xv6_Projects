#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int csfData[4];
    if (fork() == 0)
    {
        set_cfs_priority(2);
        for(int i = 1; i<= 1000000; i++){
                if(i % 100000 == 0){
                    sleep(1);
                }
            }
        get_cfs_stats(getpid(), csfData);
        printf("pid: %d, cfs: %d, rtime: %d, retime: %d, stime: %d\n", getpid(), csfData[0], csfData[1],csfData[2],csfData[3]);
    }
    else
    {
        if (fork() == 0)
        {
            set_cfs_priority(1);
            for(int i = 1; i<= 1000000; i++){
                if(i % 100000 == 0){
                    sleep(1);
                }
            }
            get_cfs_stats(getpid(), csfData);
            sleep(25);
            
            printf("pid: %d, cfs: %d, rtime: %d, retime: %d, stime: %d\n", getpid(), csfData[0], csfData[1],csfData[2],csfData[3]);
        }
        else
        {
            set_cfs_priority(0);
            for(int i = 1; i<= 1000000; i++){
                if(i % 100000 == 0){
                    sleep(1);
                }
            }
            get_cfs_stats(getpid(), csfData);
            sleep(50);
            printf("pid: %d, cfs: %d, rtime: %d, retime: %d, stime: %d\n", getpid(), csfData[0], csfData[1],csfData[2],csfData[3]);
        }
    }

    return 0;
}
  

