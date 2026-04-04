// #include "kernel/types.h"
// #include "user/user.h"

// int getvmstats(int, struct vmstats*);

// void
// print_vmstats(int pid, const char *label)
// {
//   struct vmstats info;
//   if(getvmstats(pid, &info) < 0) {
//     printf("getvmstats(%d) failed\n", pid);
//     return;
//   }
//   printf("%s - PID %d:\n", label, pid);
//   printf("  Page faults:      %d\n", info.page_faults);
//   printf("  Pages evicted:    %d\n", info.pages_evicted);
//   printf("  Pages swapped in: %d\n", info.pages_swapped_in);
//   printf("  Pages swapped out:%d\n", info.pages_swapped_out);
//   printf("  Resident pages:   %d\n", info.resident_pages);
//   printf("\n");
// }

// int
// main()
// {
//   int pid = getpid();
  
//   printf("=== Test 1: Basic getvmstats call ===\n");
//   print_vmstats(pid, "Initial stats");
  
//   printf("=== Test 2: Allocate and access memory ===\n");
//   // Allocate 1MB
//   char *ptr = sbrk(1024 * 1024);
//   if(ptr == (char*)-1) {
//     printf("sbrk failed\n");
//     exit(1);
//   }
  
//   // Access pages to trigger page faults
//   for(int i = 0; i < 1024 * 1024; i += 4096) {
//     ptr[i] = 'A' + (i / 4096) % 26;
//   }
  
//   print_vmstats(pid, "After allocating 1MB");
  
//   printf("=== Test 3: Fork and check child stats ===\n");
//   int child_pid = fork();
  
//   if(child_pid == 0) {
//     // Child process
//     print_vmstats(getpid(), "Child initial stats");
    
//     // Allocate more in child
//     char *child_ptr = sbrk(512 * 1024);
//     for(int i = 0; i < 512 * 1024; i += 4096) {
//       child_ptr[i] = 'Z' - (i / 4096) % 10;
//     }
    
//     print_vmstats(getpid(), "Child after alloc");
//     exit(0);
//   } else {
//     // Parent process
//     wait(0);
//     print_vmstats(pid, "Parent after wait");
//   }
  
//   printf("=== Test 4: Verify invalid PID ===\n");
//   struct vmstats info;
//   int result = getvmstats(99999, &info);
//   if(result < 0) {
//     printf("getvmstats(99999) correctly returned -1 for invalid PID\n");
//   } else {
//     printf("ERROR: getvmstats(99999) should return -1\n");
//   }
  
//   printf("\n=== All tests completed ===\n");
//   exit(0);
// }

// t_swap_out.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 1000;
  char *p = sbrk(pages * 4096);

  for(int i = 0; i < pages; i++)
    p[i * 4096] = i;

  struct vmstats st;
  getvmstats(getpid(), &st);

  printf("EV=%d, SO=%d (should be > 0)\n", st.pages_evicted ,st.pages_swapped_out);
}