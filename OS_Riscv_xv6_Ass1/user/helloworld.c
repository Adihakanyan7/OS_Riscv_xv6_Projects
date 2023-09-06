#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  
  // Here we write "Hello World xv6" to stdout
  write(1, "Hello World xv6\n", 17);

  exit(0, "");
  
}
