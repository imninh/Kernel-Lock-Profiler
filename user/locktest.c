#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("Running lock stress test...\n");
  
  // Tạo nhiều process để gây contention
  for(int i = 0; i < 5; i++) {
    int pid = fork();
    if(pid == 0) {
      // Child process
      for(int j = 0; j < 100; j++) {
        // Các system call này sẽ gây lock contention
        getpid();
        uptime();
      }
      exit(0);
    }
  }
  
  // Parent chờ children
  for(int i = 0; i < 10; i++) {
    wait(0);
  }
  
  printf("\nTest complete ! Run 'lockstat' to see the statistics.\n");
  
  exit(0);
}
