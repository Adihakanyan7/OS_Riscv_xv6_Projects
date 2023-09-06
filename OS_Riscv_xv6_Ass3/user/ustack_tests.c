#include "ustack.h"
#include "kernel/riscv.h"  // For macros like PGSIZE
#include "kernel/types.h"  // For types like uint
#include "user.h"   // For printf

// Maximum size of an allocation operation.
#define MAX_ALLOC_SIZE 512

void test_alloc_free(uint len) {
  printf("Testing allocation and deallocation of size %d\n", len);

  // Try to allocate a buffer of the specified size.
  void *buffer = ustack_malloc(len);
  if (len > MAX_ALLOC_SIZE) {
    // The allocation should fail.
    if ((uint64)buffer != -1) {
      printf("ERROR: Allocation of size %d succeeded but it should have failed\n", len);
    }
  } else {
    // The allocation should succeed.
    if ((uint64)buffer == -1) {
      printf("ERROR: Allocation of size %d failed but it should have succeeded\n", len);
    } else {
      // Try to free the buffer.
      int freed_len = ustack_free();
      if (freed_len != len) {
        printf("ERROR: Freed buffer has length %d but it should have length %d\n", freed_len, len);
      }
    }
  }
}

void test_alloc_dealloc_page() {
  printf("Testing allocation and deallocation of a full page\n");

  // Calculate the number of buffers that fit in a page.
  uint num_buffers = PGSIZE / (sizeof(struct uheader) + MAX_ALLOC_SIZE);
 
  // Try to allocate the buffers.
  for (uint i = 0; i < num_buffers; i++) {
    void *buffer = ustack_malloc(MAX_ALLOC_SIZE);
    if ((uint64)buffer == -1) {
      printf("ERROR: Allocation of buffer %d failed\n", i);
    }
  }
 
  // Try to free the buffers.
  for (uint i = 0; i < num_buffers; i++) {
    int freed_len = ustack_free();
    if (freed_len != MAX_ALLOC_SIZE) {
      printf("ERROR: Freed buffer has length %d but it should have length %d\n", freed_len, MAX_ALLOC_SIZE);
    }
  }
}

int main(void) {
  // Test allocation and deallocation of different sizes.
  for (uint len = 0; len <= MAX_ALLOC_SIZE + 100; len += 100) {
    test_alloc_free(len);
  }

  // Test allocation and deallocation of a full page.
  test_alloc_dealloc_page();

  exit(0);
}











