// t_integrity.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 800;
  char *p = sbrk(pages * 4096);

  for(int i = 0; i < pages; i++)
    p[i * 4096] = i % 256;

  for(int i = 0; i < pages; i++){
    if(p[i * 4096] != (i % 256)){
      printf("ERROR at %d\n", i);
      exit(1);
    }
  }

  printf("PASS\n");
}