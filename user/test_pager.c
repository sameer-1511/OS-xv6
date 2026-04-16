#include "kernel/types.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define NUM_PAGES 256   // Allocate enough to trigger page replacement

int
main()
{
    printf("=== Test 1: Basic Page Replacement ===\n");
    
    int pid = getpid();
    struct vmstats stats_before, stats_after;
    
    // Get initial stats
    if(getvmstats(pid, &stats_before) < 0) {
        printf("ERROR: getvmstats failed\n");
        exit(1);
    }
    
    printf("Initial Stats:\n");
    printf("  Page Faults: %d\n", stats_before.page_faults);
    printf("  Pages Evicted: %d\n", stats_before.pages_evicted);
    printf("  Pages Swapped In: %d\n", stats_before.pages_swapped_in);
    printf("  Pages Swapped Out: %d\n", stats_before.pages_swapped_out);
    printf("  Resident Pages: %d\n\n", stats_before.resident_pages);
    
    // Allocate large memory region using sbrk
    printf("Allocating %d pages (%d bytes)...\n", NUM_PAGES, NUM_PAGES * PAGE_SIZE);
    char* base = sbrk(NUM_PAGES * PAGE_SIZE);
    if(base == (char*)-1) {
        printf("ERROR: sbrk failed\n");
        exit(1);
    }
    
    // Touch all pages to trigger page faults and replacements
    printf("Accessing all pages sequentially...\n");
    for(int i = 0; i < NUM_PAGES; i++) {
        base[i * PAGE_SIZE] = (char)(i % 256);
        if(i % 32 == 0) printf("  Accessed page %d\n", i);
    }
    
    printf("\nVerifying page contents...\n");
    // Verify that pages are still accessible (either in memory or successfully restored from swap)
    int errors = 0;
    for(int i = 0; i < NUM_PAGES; i++) {
        char expected = (char)(i % 256);
        if(base[i * PAGE_SIZE] != expected) {
            errors++;
        }
    }
    
    if(errors > 0) {
        printf("ERROR: %d pages have incorrect content\n", errors);
        exit(1);
    }
    printf("All pages verified successfully!\n\n");
    
    // Get final stats
    if(getvmstats(pid, &stats_after) < 0) {
        printf("ERROR: getvmstats failed\n");
        exit(1);
    }
    
    printf("Final Stats:\n");
    printf("  Page Faults: %d (expected > 0)\n", stats_after.page_faults);
    printf("  Pages Evicted: %d (expected > 0)\n", stats_after.pages_evicted);
    printf("  Pages Swapped In: %d (expected > 0)\n", stats_after.pages_swapped_in);
    printf("  Pages Swapped Out: %d (expected > 0)\n", stats_after.pages_swapped_out);
    printf("  Resident Pages: %d\n\n", stats_after.resident_pages);
    
    // Verify statistics make sense
    if(stats_after.page_faults <= stats_before.page_faults) {
        printf("WARNING: Page faults did not increase\n");
    }
    if(stats_after.pages_evicted <= stats_before.pages_evicted) {
        printf("WARNING: Pages evicted did not increase\n");
    }
    if(stats_after.pages_swapped_out <= stats_before.pages_swapped_out) {
        printf("WARNING: Pages swapped out did not increase\n");
    }
    if(stats_after.pages_swapped_in <= stats_before.pages_swapped_in) {
        printf("WARNING: Pages swapped in did not increase\n");
    }
    
    printf("Test 1 completed successfully!\n");
    exit(0);
}
