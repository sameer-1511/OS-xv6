// t_basic.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  char *p = sbrk(4096 * 10); // 10 pages

  for(int i = 0; i < 10; i++)
    p[i * 4096] = i;

  struct vmstats st;
  getvmstats(getpid(), &st);

  printf("PF=%d EV=%d RP=%d\n", st.page_faults, st.pages_evicted, st.resident_pages);
}