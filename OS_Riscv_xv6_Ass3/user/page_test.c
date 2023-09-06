#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


#define PGSIZE 4096
#define NUM_PAGES 10

// Allocate and touch a number of pages.
void alloc_touch_pages(int num_pages) {
  int i;
  for (i = 0; i < num_pages; i++) {
    char *page = malloc(PGSIZE);
    if (!page) {
      printf("###### Failed to allocate page %d\n", i);
      exit(0);
    }

    // Write to the page to ensure it's actually allocated.
    page[0] = 42;
    page[PGSIZE - 1] = 42;
  }
}

// Test the swapping mechanism by allocating a large number of pages and accessing them.
void swapping_test() {
  printf("###### Running swapping test...\n");

  alloc_touch_pages(NUM_PAGES);

  printf("###### Swapping test completed successfully\n");
}

void fork_test() {
  printf("Running fork test...\n");

  //int parent_pid = getpid();
  char *page = malloc(PGSIZE);
  page[0] = 42;
  if (fork() == 0) {
    // Child process
    page[0] = 1;
    printf("Child has written to its memory. Exiting child process...\n");
    exit(0);
  } else {
    // Parent process
    int exit_status;
    wait(&exit_status);
    printf("Child exited with status: %d\n",exit_status);
    if(page[0] != 42) {
      printf("Parent memory was affected by child process! Fork test failed.\n");
    } else {
      printf("Fork test completed successfully\n");
    }
  }
}

void exec_test() {
  printf("Running exec test...\n");

  // Write something into the current image.
  char *page = malloc(PGSIZE);
  page[0] = 42;

  // Create a new process image.
  if (fork() == 0) {
    // Child process
    char *argv[] = {"echo", "Hello, World!", 0};
    exec("/echo", argv);
    printf("Exec failed!\n");
    exit(0);
  } else {
    // Parent process
    int exit_status;
    wait(&exit_status);
    printf("Child exited with status: %d\n",exit_status);
    if(page[0] != 42) {
      printf("Memory was affected by exec! Exec test failed.\n");
    } else {
      printf("Exec test completed successfully\n");
    }
  }
}

int main(int argc, char *argv[]) {
  swapping_test();
  fork_test();
  exec_test();
  exit(0);
}