#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int policy = atoi(argv[1]);
    if(policy > 2 || policy < 0) // policy can only be 0,1 or 2. otherwise - incorrect.
        printf("Error - invalid policy value. please enter 0, 1 or 2.\n");
    else{
        set_policy(policy);
        printf("Success on performing set_policy system call !\n");
    }
    exit(0,"");
}