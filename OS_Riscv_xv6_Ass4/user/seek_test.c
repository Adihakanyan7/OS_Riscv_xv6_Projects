#ifndef TESTS_H
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#endif

int test_seek(int print) {
  int fd = open("testfile.txt", O_RDWR | O_CREATE | O_TRUNC);
  if (fd < 0) {
    printf("error: open test file on seek test\n");
    return 0;
  }

  char expected1[] = "Hello, World!\n";
  char expected2[] = "Hello, World!\n\0This is new data!\n";
  char expected3[] = "ta!\n";
  char expected4[] = "Hello, World!\n\0This is new data!\n\0Extra fresh data!\n";

  // Write some data to the beginning of the file
  seek(fd, 0, SEEK_SET);
  char write1[] = "Hello, World!\n";
  write(fd, write1, sizeof(write1));

  // Seek to the beginning of the file to read and print the written data
  seek(fd, 0, SEEK_SET);
  char actual1[20];
  read(fd, actual1, sizeof(actual1));
  if (memcmp(expected1, actual1, strlen(expected1)) == 0)
  {
    if (print) 
      printf("Writing and reading \"Hello, World!\\n\" successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE at writing or reading \"Hello, World!\\n\"\n\n");
    close(fd);
    return 0;
  }

  if (print) printf("File content after first write:\n");
  for(int i = 0; i < sizeof(actual1); i++)
    if (print) printf("%c", actual1[i]);
  if (print) printf("\n");

  // Seek to the end of the file to write additional data
  seek(fd, 0, SEEK_CUR);
  char write2[] = "This is new data!\n";
  write(fd, write2, sizeof(write2));

  // Testing serek with negative offset whose absolute value is larger than filesize
  // should seek to offset 0
  seek(fd, -50, SEEK_CUR);
  
  // Read the updated data from the beginning of the file
  char actual2[40];
  read(fd, actual2, sizeof(actual2));

  if (memcmp(expected2, actual2, sizeof(expected2)) == 0)
  {
    if (print) printf("Concatenating \"This is new data!\\n\" successful!\n\n");
  }
  else
  {
    printf("ERROR: FAILURE to concatenate or read entire data from the file\n\n");
    close(fd);
    return 0;
  }
  if (print) printf("File content after second write:\n");
  for(int i = 0; i < sizeof(actual2); i++)
    if (print) printf("%c", actual2[i]);
  if (print) printf("\n");

  // Seek slightly backwards then re-read
  seek(fd, -5, SEEK_CUR);
  char actual3[40];
  read(fd, actual3, sizeof(actual3));
  if (memcmp(expected3, actual3, sizeof(expected3)) == 0)
  {
    if (print) printf("Moving backwards 5 bytes then reading is successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE to go back 5 bytes and read content properly\n\n");
    close(fd);
    return 0;
  }
  if (print) printf("Read after going slightly backwards:\n%s\n", actual3);

  // Go too far to write new stuff
  seek(fd, 500, SEEK_SET);
  char write3[] = "Extra fresh data!\n";
  write(fd, write3, sizeof(write3));

  seek(fd, 0, SEEK_SET);
  char actual4[70];
  read(fd, actual4, sizeof(actual4));

  if (memcmp(expected4, actual4, sizeof(expected4)) == 0)
  {
    if (print) printf("Concatenating \"Extra fresh data!\\n\" successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE to concatenate or read extra data\n\n");
    close(fd);
    return 0;
  }

  if (print) printf("File content after third write:\n");
  for(int i = 0; i < sizeof(actual4); i++)
    if (print) printf("%c", actual4[i]);
  if (print) printf("\n");

  printf("SUCCESS!! PASSED SEEK TETSTS\n");
  close(fd);
  return 1;
}