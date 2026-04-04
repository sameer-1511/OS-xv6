/*// t_clock.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 300;
  char *p = sbrk(pages * 4096);

  for(int i = 0; i < pages; i++)
    p[i * 4096] = i;

  // repeatedly access first 50 pages
  for(int r = 0; r < 10; r++){
    for(int i = 0; i < 50; i++)
      p[i * 4096]++;
  }

  // now access all again
  int errors = 0;
  for(int i = 0; i < pages; i++){
    if(p[i * 4096] == 0)
      errors++;
  }

  printf("Errors: %d\n", errors);
}
*/


// t_multi.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  if(fork() == 0){
    char *p = sbrk(800 * 4096);
    for(int i = 0; i < 800; i++)
      p[i * 4096] = 1;
    pause(100);
    exit(0);
  } else {
    char *p = sbrk(800 * 4096);
    for(int i = 0; i < 800; i++)
      p[i * 4096] = 2;
    wait(0);
  }
}