// #include "kernel/types.h"
// #include "user/user.h"

// int getvmstats(int, struct vmstats*);

// void
// print_vmstats_compact(int pid)
// {
//   struct vmstats info;
//   if(getvmstats(pid, &info) < 0) {
//     printf("ERROR: getvmstats failed\n");
//     return;
//   }
//   printf("PF:%d EV:%d SI:%d SO:%d RP:%d | ",
//          info.page_faults, info.pages_evicted,
//          info.pages_swapped_in, info.pages_swapped_out,
//          info.resident_pages);
// }

// int
// main(int argc, char *argv[])
// {
//   int alloc_size = 4;  // MB, can be passed as argument
//   if(argc > 1) {
//     alloc_size = atoi(argv[1]);
//   }
  
//   int pid = getpid();
//   printf("=== Memory Pressure Test ===\n");
//   printf("Allocating %d MB progressively...\n\n", alloc_size);
  
//   struct vmstats initial;
//   getvmstats(pid, &initial);
//   printf("Initial: ");
//   print_vmstats_compact(pid);
//   printf("\n");
  
//   // Allocate memory progressively in 1MB chunks
//   for(int chunk = 0; chunk < alloc_size; chunk++) {
//     char *ptr = sbrk(1024 * 1024);
//     if(ptr == (char*)-1) {
//       printf("sbrk failed at chunk %d\n", chunk);
//       break;
//     }
    
//     // Touch every page to trigger allocation
//     printf("Chunk %d (written): ", chunk);
//     for(int i = 0; i < 1024 * 1024; i += 4096) {
//       ptr[i] = (char)('A' + chunk);
//     }
//     print_vmstats_compact(pid);
//     printf("\n");
//   }
  
//   struct vmstats final_stats;
//   getvmstats(pid, &final_stats);
  
//   printf("\n=== Summary ===\n");
//   printf("Page faults:      %d (initial: %d, delta: %d)\n",
//          final_stats.page_faults, initial.page_faults,
//          final_stats.page_faults - initial.page_faults);
//   printf("Pages evicted:    %d\n", final_stats.pages_evicted);
//   printf("Pages swapped:    %d out, %d in\n",
//          final_stats.pages_swapped_out,
//          final_stats.pages_swapped_in);
//   printf("Resident pages:   %d\n", final_stats.resident_pages);
  
//   if(final_stats.pages_evicted > 0) {
//     printf("\n✓ Page eviction occurred (memory pressure worked)\n");
//   } else {
//     printf("\n✗ No page eviction detected (may have enough memory)\n");
//   }
  
//   exit(0);
// }

// t_frame_limit.c
#include "kernel/types.h"
#include "user/user.h"

int main() {
  int pages = 2000;
  char *p = sbrk(pages * 4096);

  for(int i = 0; i < pages; i++)
    p[i * 4096] = 1;

  struct vmstats st;
  getvmstats(getpid(), &st);

  printf("RP=%d (should be <= 256)\n", st.resident_pages);
}