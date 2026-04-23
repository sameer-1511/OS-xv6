#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// struct diskstats {
//   uint64 disk_reads;
//   uint64 disk_writes;
//   uint64 avg_disk_latency;
// };

// Force disk activity by doing lots of swap
#define NPAGES 150
#define PGSIZE 4096

static void run_workload(char *label) {
  char *pages[NPAGES];

  for(int i = 0; i < NPAGES; i++) {
    pages[i] = sbrk(PGSIZE);
    pages[i][0] = i;
  }
  // random-order access to stress scheduler
  for(int i = NPAGES-1; i >= 0; i -= 2) pages[i][0]++;
  for(int i = 0; i < NPAGES; i += 3)    pages[i][0]++;

  struct diskstats ds;
  getdiskstats(&ds);
  printf("[%s] reads=%d writes=%d avg_latency=%d (x100)\n",
    label,
    (int)ds.disk_reads,
    (int)ds.disk_writes,
    (int)ds.avg_disk_latency);
}

int main() {
  printf("=== Disk Scheduling Test ===\n");

  printf("--- Policy: FCFS ---\n");
  setdisksched(0);
  run_workload("FCFS");

  printf("--- Policy: SSTF ---\n");
  setdisksched(1);
  run_workload("SSTF");

  printf("NOTE: SSTF avg_latency should be <= FCFS avg_latency\n");
  exit(0);
}