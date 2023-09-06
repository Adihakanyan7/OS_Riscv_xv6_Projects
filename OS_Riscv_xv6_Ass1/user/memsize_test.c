#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("memsize test\n");

  int ans = memsize();
  char *adder;
  printf("initial process memsize in bytes is : %d\n", ans);
  
  adder = (char *)malloc(20000);
  ans = memsize();

  printf("process memsize in bytes after allocating 20000 is : %d\n", ans);

  free(adder);
  ans = memsize();

  printf("process memsize in bytes after freeing the allocated space is : %d\n", ans);

  exit(0, "");
  
}