#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// struct diskstats {
//   uint64 disk_reads;
//   uint64 disk_writes;
//   uint64 avg_disk_latency;
// };

#define NPAGES 150
#define PGSIZE 4096

int main() {
  printf("=== Statistics Test ===\n");

  struct diskstats before, after;
  getdiskstats(&before);
  printf("Before: reads=%lu writes=%lu latency=%lu\n",
    before.disk_reads, before.disk_writes,
    before.avg_disk_latency);

  // Trigger swap activity
  char *pages[NPAGES];
  for(int i = 0; i < NPAGES; i++) {
    pages[i] = sbrk(PGSIZE);
    pages[i][0] = i;
  }
  for(int i = NPAGES-1; i >= 0; i--)
    pages[i][0]++;  // force swap-in

  getdiskstats(&after);
  printf("After:  reads=%lu writes=%lu latency=%lu\n",
    after.disk_reads, after.disk_writes,
    after.avg_disk_latency);

  // Validate counters increased
  if(after.disk_reads > before.disk_reads)
    printf("PASS: disk_reads increased\n");
  else
    printf("FAIL: disk_reads did not increase\n");

  if(after.disk_writes > before.disk_writes)
    printf("PASS: disk_writes increased\n");
  else
    printf("FAIL: disk_writes did not increase\n");

  if(after.avg_disk_latency > 0)
    printf("PASS: avg_latency = %d (x100 fixed-point)\n",
      (int)after.avg_disk_latency);
  else
    printf("FAIL: avg_latency is 0\n");

  exit(0);
}