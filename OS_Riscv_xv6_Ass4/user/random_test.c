#ifndef TESTS_H
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#endif

// Linear feedback shift register
// Returns the next pseudo-random number
// The seed is updated with the returned value
uint8 lfsr_char(uint8 lfsr)
{
  uint8 bit;
  bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
  lfsr = (lfsr >> 1) | (bit << 7);
  return lfsr;   
}

int test_random(int reset, int print) {
  int fd = open("random", O_RDWR);
  if (fd < 0) {
  printf("error: open\n");
  return 0;
  }
  
  // IMPORTANT NOTE: RE-RUNNING THIS TEST SHOULD FAIL! 
  // BY WRITING A NEW SEED AT THE END OF THE TEST, 
  // THE DRIVER SHOULD HAVE A NEW SEED AT THE END, 
  // RUN TESTS WITH RESET > 0 TO RESET TO THE ORIGINAL SEED EVERY RUN
  if (reset)
  {
      uint8 originalSeed = 0x2A;
      int writeResult = write(fd, &originalSeed, 1);
      if (writeResult == -1)
      {
          printf("ERROR ON RESETING TO ORIGINAL SEED\n");
          return 0;
      }
      else printf("RESET SEED TO THE ORIGINAL SEED!\n");
  }

  int expected[255];
  expected[0] = lfsr_char(0x2A);
  for(int i = 1; i < 255; i++)
  {
    expected[i] = lfsr_char(expected[i-1]);
  }

  uint8 res;
  read(fd, &res, 1);
  if (res == expected[0])
  {   
      if (print) printf("Success on generating first pseudo-random number from original seed\n");
  }
  else
    {
      printf("ERORR on generating first pseudo-random number from original seed\n");
      return 0;
    }
  // Make sure there are no errors in generating following prng values
  for(int i = 1; i < 255; i++)
  {
      int ret = read(fd, &res, 1);
      if (ret != 1 || res != expected[i])
      {
        printf("ERROR on generating new pseudo-random number from previous number\n");
        close(fd);
        return 0;
      }
  }
    
  if (print) printf("Successfully generated all 255 variants of the original seed!\n");
  read(fd, &res, 1);
  if (res == expected[0])
    {
      if (print) printf("Successfully looped through all values and returned to initial value!\n");
    }
  else
  {
    printf("ERROR on looping through all values and returning to initial value\n");
    close(fd);
    return 0;
  }

  // Test changing the seed and getting new stuff:
  uint8 val = 0x4C;
  int writeResult = write(fd, (void*)&val, 6);
  if (writeResult != -1)
  {
    printf("ERROR on writing new seed with n != 1\n");
    close(fd);
    return 0;
  }
  writeResult = write(fd, (void*)&val, 1);
  if (writeResult == -1)
  {
    printf("ERROR on writing new seed which should've succeeded\n");
    close(fd);
    return 0;
  }
  if (print) printf("Writing new seed 0x4C successfull\n");
  
  int lfsrOfNewSeed = lfsr_char(0x4C);
  read(fd, &res, 1);
  if (res != lfsrOfNewSeed)
  {
    printf("ERROR on generating new PRNG\n");
    close(fd);
    return 0;
  }
  printf("SUCCESS!! PASSED RANDOM TESTS!\n");
  close(fd);
  return 1;
}