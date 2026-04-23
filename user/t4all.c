#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
  printf("\n=============================\n");
  printf("  PA4 Full Test Suite\n");
  printf("=============================\n\n");

  int pid;

  // Test 1: swap correctness
  printf("[1/4] Swap correctness...\n");
  pid = fork();
  if(pid == 0) { exec("t4swap", (char*[]){"t4swap", 0}); exit(1); }
  int status; wait(&status);
  printf(status == 0 ? "  => PASSED\n\n" : "  => FAILED\n\n");

  // Test 2: disk scheduling
  printf("[2/4] Disk scheduling (FCFS vs SSTF)...\n");
  pid = fork();
  if(pid == 0) { exec("t4sched", (char*[]){"t4sched", 0}); exit(1); }
  wait(&status);
  printf(status == 0 ? "  => PASSED\n\n" : "  => FAILED\n\n");

  // Test 3: RAID modes
  printf("[3/4] RAID mode data integrity...\n");
  pid = fork();
  if(pid == 0) { exec("t4raid", (char*[]){"t4raid", 0}); exit(1); }
  wait(&status);
  printf(status == 0 ? "  => PASSED\n\n" : "  => FAILED\n\n");

  // Test 4: statistics
  printf("[4/4] Kernel statistics...\n");
  pid = fork();
  if(pid == 0) { exec("t4stats", (char*[]){"t4stats", 0}); exit(1); }
  wait(&status);
  printf(status == 0 ? "  => PASSED\n\n" : "  => FAILED\n\n");

  printf("=============================\n");
  printf("  Done.\n");
  printf("=============================\n");
  exit(0);
}