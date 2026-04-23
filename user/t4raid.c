#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPAGES 30
#define PGSIZE 4096

static int test_mode(int mode, char *name) {
  setraidmode(mode);
  printf("--- RAID mode: %s ---\n", name);

  char *pages[NPAGES];
  for(int i = 0; i < NPAGES; i++) {
    pages[i] = sbrk(PGSIZE);
    if(pages[i] == (char*)-1) {
      printf("FAIL: sbrk failed at page %d\n", i);
      return 1;
    }
    pages[i][0]        = (char)(i * 3);
    pages[i][PGSIZE/2] = (char)(i * 7);
    pages[i][PGSIZE-1] = (char)(i * 13);
  }

  // Force swap-in by accessing in reverse
  int errors = 0;
  for(int i = NPAGES-1; i >= 0; i--) {
    if(pages[i][0]        != (char)(i * 3))  errors++;
    if(pages[i][PGSIZE/2] != (char)(i * 7))  errors++;
    if(pages[i][PGSIZE-1] != (char)(i * 13)) errors++;
  }

  if(errors == 0)
    printf("PASS: %s data integrity OK\n", name);
  else
    printf("FAIL: %s had %d data errors\n", name, errors);
  return errors;
}

int main() {
  printf("=== RAID Mode Test ===\n");

  int total = 0;
  total += test_mode(0, "RAID0 (striping)");
  total += test_mode(1, "RAID1 (mirroring)");
  total += test_mode(2, "RAID5 (parity)");

  if(total == 0)
    printf("ALL RAID MODES PASSED\n");
  else
    printf("TOTAL ERRORS: %d\n", total);

  exit(0);
}