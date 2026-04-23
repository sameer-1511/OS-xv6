#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPAGES 50
#define PGSIZE 4096

int main() {
  printf("=== Swap Test ===\n");
  
  // Allocate way more than MAXFRAMES to force eviction
  char *pages[NPAGES];
  for(int i = 0; i < NPAGES; i++) {
    pages[i] = sbrk(PGSIZE);
    if(pages[i] == (char*)-1) {
      printf("sbrk failed at %d\n", i);
      exit(1);
    }
    pages[i][0] = (char)(i & 0xFF);  // write a known value
    pages[i][PGSIZE-1] = (char)(~i & 0xFF);
  }

  printf("Allocated %d pages, verifying data after swap...\n", NPAGES);

  // Now read them all back — forces swap-in of evicted pages
  int errors = 0;
  for(int i = 0; i < NPAGES; i++) {
    if(pages[i][0] != (char)(i & 0xFF)) {
      printf("FAIL page %d: expected %d got %d\n", i, i & 0xFF, (unsigned char)pages[i][0]);
      errors++;
    }
    if(pages[i][PGSIZE-1] != (char)(~i & 0xFF)) {
      printf("FAIL page %d tail: expected %d got %d\n", i, ~i & 0xFF, (unsigned char)pages[i][PGSIZE-1]);
      errors++;
    }
  }

  if(errors == 0)
    printf("PASS: all %d pages verified correctly after swap\n", NPAGES);
  else
    printf("FAIL: %d errors found\n", errors);

  exit(0);
}