// t_eviction.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 400;
  char *p = sbrk(pages * 4096);

  for(int i = 0; i < pages; i++)
    p[i * 4096] = i;

  struct vmstats st;
  getvmstats(getpid(), &st);

  printf("EV=%d (should be > 0)\n", st.pages_evicted);
}
