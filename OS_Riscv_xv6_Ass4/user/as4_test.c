#ifndef TESTS_H
#define TESTS_H 
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "random_test.c"
#include "seek_test.c"
#endif


int main(int argc, char** argv)
{
    // Reset flag = reset PRG to original seed 0x2A
    int reset = 0;
    // print flag = print non-errors during sub-tests
    int print = 0;
    for(int i = 0; i < argc; i++)
    {
        if (strcmp("-r", argv[i]) == 0)
            reset++;
        if (strcmp("-p", argv[i]) == 0)
            print++;
    }
    
    int success = 0;
    printf("SEEK TESTS:\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    success += test_seek(print);
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("\nPRNG TESTS:\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    success += test_random(reset, print);
    printf("\n\n");
    if (success == 2)
        printf("SUCCESS!! PASSED ALL TESTS!\n");
    else
        printf("FAILED 1 OR MORE TESTS\n");
    return 0;
}