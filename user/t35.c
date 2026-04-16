// t_swap_in.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 600;
  char *p = sbrk(pages * 4096);

  // write
  for(int i = 0; i < pages; i++)
    p[i * 4096] = i;

  // access again (forces swap-in)
  int sum = 0;
  for(int i = 0; i < pages; i++)
    sum += p[i * 4096];

  struct vmstats st;
  getvmstats(getpid(), &st);

  printf("PF=%d EV=%d SI=%d SO=%d RI=%d(SI should be > 0)\n", st.page_faults, st.pages_evicted, st.pages_swapped_in, st.pages_swapped_out, st.resident_pages);
}