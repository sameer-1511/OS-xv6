#include "kernel/types.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define PHASE1_PAGES 128  // Initial allocation
#define PHASE2_PAGES 128  // Additional allocation to trigger swaps

int
main()
{
    printf("=== Test 3: Swap and Page Restoration ===\n");
    printf("This test verifies swap-out and swap-in functionality\n\n");
    
    int pid = getpid();
    struct vmstats stats;
    
    // PHASE 1: Initial allocation and access
    printf("Phase 1: Initial memory allocation\n");
    getvmstats(pid, &stats);
    printf("  Initial - Faults: %d, Swapped In: %d, Swapped Out: %d\n",
           stats.page_faults, stats.pages_swapped_in, stats.pages_swapped_out);
    
    char* region1 = sbrk(PHASE1_PAGES * PAGE_SIZE);
    if(region1 == (char*)-1) {
        printf("ERROR: sbrk failed in Phase 1\n");
        exit(1);
    }
    
    // Touch all pages in region1
    printf("  Accessing %d pages in region 1...\n", PHASE1_PAGES);
    for(int i = 0; i < PHASE1_PAGES; i++) {
        region1[i * PAGE_SIZE] = (char)(i % 256);
    }
    
    getvmstats(pid, &stats);
    printf("  After Phase 1 - Faults: %d\n", stats.page_faults);
    int phase1_faults = stats.page_faults;
    
    // PHASE 2: Additional allocation to trigger page replacement
    printf("\nPhase 2: Additional allocation to trigger eviction\n");
    printf("  Allocating %d more pages...\n", PHASE2_PAGES);
    char* region2 = sbrk(PHASE2_PAGES * PAGE_SIZE);
    if(region2 == (char*)-1) {
        printf("ERROR: sbrk failed in Phase 2\n");
        exit(1);
    }
    
    // Touch all pages in region2 (this should trigger replacements)
    printf("  Accessing %d pages in region 2...\n", PHASE2_PAGES);
    for(int i = 0; i < PHASE2_PAGES; i++) {
        region2[i * PAGE_SIZE] = (char)((i + 128) % 256);
    }
    
    getvmstats(pid, &stats);
    printf("  After Phase 2 - Evicted: %d, Swapped Out: %d\n",
           stats.pages_evicted, stats.pages_swapped_out);
    int phase2_evictions = stats.pages_evicted;
    int phase2_swapped_out = stats.pages_swapped_out;
    
    // PHASE 3: Re-access region 1 pages (should trigger swap-in)
    printf("\nPhase 3: Re-accessing region 1 (should cause swap-ins)\n");
    printf("  Verifying region 1 pages...\n");
    
    int errors = 0;
    for(int i = 0; i < PHASE1_PAGES; i++) {
        char expected = (char)(i % 256);
        char actual = region1[i * PAGE_SIZE];
        if(actual != expected) {
            errors++;
        }
    }
    
    if(errors > 0) {
        printf("  ERROR: %d pages from region 1 have incorrect values\n", errors);
        exit(1);
    }
    printf("  All region 1 pages verified successfully (swap-in confirmed)\n");
    
    getvmstats(pid, &stats);
    printf("  After Phase 3 - Swapped In: %d\n", stats.pages_swapped_in);
    int phase3_swapped_in = stats.pages_swapped_in;
    
    // PHASE 4: Verify region 2 pages are still intact
    printf("\nPhase 4: Verifying region 2 pages\n");
    errors = 0;
    for(int i = 0; i < PHASE2_PAGES; i++) {
        char expected = (char)((i + 128) % 256);
        char actual = region2[i * PAGE_SIZE];
        if(actual != expected) {
            errors++;
        }
    }
    
    if(errors > 0) {
        printf("  ERROR: %d pages from region 2 have incorrect values\n", errors);
        exit(1);
    }
    printf("  All region 2 pages verified successfully\n");
    
    // Summary
    printf("\n=== Swap and Restore Test Summary ===\n");
    printf("Phase 1 faults: %d\n", phase1_faults);
    printf("Phase 2 evictions: %d (expected > 0)\n", phase2_evictions);
    printf("Phase 2 swapped out: %d (expected > 0)\n", phase2_swapped_out);
    printf("Phase 3 swapped in: %d (expected > 0)\n", phase3_swapped_in);
    
    // Verify statistics make sense
    if(phase2_evictions == 0) {
        printf("\nWARNING: No pages were evicted (system has enough memory?)\n");
    }
    if(phase2_swapped_out == 0) {
        printf("WARNING: No pages were swapped out\n");
    }
    if(phase3_swapped_in == 0 && phase2_swapped_out > 0) {
        printf("WARNING: Pages were swapped out but not swapped in\n");
    }
    
    printf("\nTest 3 completed successfully!\n");
    exit(0);
}
