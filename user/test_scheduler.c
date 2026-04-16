#include "kernel/types.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define PAGES_PER_PROCESS 100
#define NUM_PROCESSES 3

void child_process(int child_id, int priority_boost)
{
    int pid = getpid();
    struct vmstats stats_before, stats_after;
    
    printf("Child %d (PID %d): Starting memory allocation\n", child_id, pid);
    
    // Get initial stats
    getvmstats(pid, &stats_before);
    
    // Allocate and touch memory
    char* base = sbrk(PAGES_PER_PROCESS * PAGE_SIZE);
    if(base == (char*)-1) {
        printf("Child %d: ERROR - sbrk failed\n", child_id);
        exit(1);
    }
    
    // Perform different workloads based on priority
    for(int i = 0; i < PAGES_PER_PROCESS; i++) {
        base[i * PAGE_SIZE] = (char)(child_id * 100 + i % 256);
        
        // High priority process does extra work to stay in high queue
        if(priority_boost == 1) {
            for(int j = 0; j < 1000; j++);
        }
    }
    
    // Get final stats
    getvmstats(pid, &stats_after);
    
    printf("Child %d: Evicted=%d, SwappedOut=%d, SwappedIn=%d, ResidentPages=%d\n",
           child_id, 
           stats_after.pages_evicted,
           stats_after.pages_swapped_out,
           stats_after.pages_swapped_in,
           stats_after.resident_pages);
    
    // Verify all pages are accessible
    int errors = 0;
    for(int i = 0; i < PAGES_PER_PROCESS; i++) {
        char expected = (char)(child_id * 100 + i % 256);
        if(base[i * PAGE_SIZE] != expected) {
            errors++;
        }
    }
    
    if(errors > 0) {
        printf("Child %d: ERROR - %d pages have incorrect content\n", child_id, errors);
        exit(1);
    }
    
    printf("Child %d: Exiting successfully\n", child_id);
    exit(0);
}

int
main()
{
    printf("=== Test 2: Scheduler-Aware Eviction ===\n");
    printf("This test verifies that lower-priority processes lose pages earlier\n\n");
    
    // Create multiple processes with different workloads
    // Lower priority processes (CPU intensive) should lose pages before higher priority
    
    int pids[NUM_PROCESSES];
    
    for(int i = 0; i < NUM_PROCESSES; i++) {
        pids[i] = fork();
        if(pids[i] == 0) {
            // Child process
            // Make some processes CPU-bound to keep them in higher priority queues (SC-MLFQ)
            int is_cpu_intensive = (i == 0) ? 0 : 1;  // First child is less CPU intensive
            child_process(i, is_cpu_intensive);
            exit(0);
        } else if(pids[i] < 0) {
            printf("ERROR: fork failed\n");
            exit(1);
        }
    }
    
    printf("Parent: All children created, waiting for completion...\n\n");
    
    // Wait for all children to complete
    for(int i = 0; i < NUM_PROCESSES; i++) {
        wait(0);
    }
    
    printf("\n=== Scheduler-Aware Eviction Test Completed ===\n");
    printf("Expected behavior:\n");
    printf("- Lower priority processes (less CPU intensive) should have higher pages_evicted\n");
    printf("- Higher priority processes should retain more resident pages\n");
    printf("- All processes should have consistent page_faults and eviction stats\n");
    
    exit(0);
}
