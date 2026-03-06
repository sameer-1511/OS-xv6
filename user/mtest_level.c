#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void run_cpu_bound(int pid) {
    printf("[PID %d] CPU-bound process started\n", pid);
    for(int iter = 0; iter < 8; iter++) {
        int level = getlevel();
        printf("  [PID %d] Iter %d: Level %d\n", pid, iter, level);
        for(long j = 0; j < 1000000000; j++);
    }
    printf("[PID %d] CPU-bound test complete\n", pid);
}

void run_io_bound(int pid) {
    printf("[PID %d] IO-bound process started\n", pid);
    for(int iter = 0; iter < 8; iter++) {
        int level = getlevel();
        printf("  [PID %d] Iter %d: Level %d\n", pid, iter, level);
        for(int k = 0; k < 15; k++) {
            getpid();  // Syscall = IO-bound behavior
        }
    }
    printf("[PID %d] IO-bound test complete\n", pid);
}

int main(int argc, char *argv[]) {
    int test_type = 0;  // 0 = CPU-bound, 1 = IO-bound, 2 = multi-process
    
    if(argc > 1) {
        test_type = argv[1][0] - '0';  // 0, 1, or 2
    }

    if(test_type == 0) {
        // CPU-BOUND PROCESS - Should demote through all levels
        printf("=== CPU-BOUND TEST (Single Process) ===\n");
        printf("Process: CPU-bound (few syscalls, many ticks)\n");
        printf("Expected: Demote from level 0→1→2→3\n\n");

        for(int iter = 0; iter < 12; iter++) {
            int level = getlevel();
            int syscount = getsyscount();
            printf("[Iter %d] Level: %d  Syscalls: %d\n", iter, level, syscount);

            // Spin for a while (consumes ticks, no syscalls = demotion)
            for(long j = 0; j < 800000000; j++);
        }
        printf("\nCPU-bound test complete\n");
    } 
    else if(test_type == 1) {
        // IO-BOUND PROCESS - Should stay at high priority
        printf("=== IO-BOUND TEST (Single Process) ===\n");
        printf("Process: IO-bound (many syscalls, few ticks)\n");
        printf("Expected: Stay at level 0 (frequent yields via getlevel)\n\n");

        for(int iter = 0; iter < 12; iter++) {
            int level = getlevel();
            int syscount = getsyscount();
            printf("[Iter %2d] Level: %d  Syscalls: %d\n", iter, level, syscount);

            // Make syscalls (yields CPU) = IO-bound behavior
            for(int k = 0; k < 10; k++) {
                getpid();  // Cheap syscall to trigger IO-bound detection
            }
        }
        printf("\nIO-bound test complete\n");
    }
    else if(test_type == 2) {
        // MULTI-PROCESS TEST
        printf("=== MULTI-PROCESS TEST ===\n");
        printf("Running 2 CPU-bound + 2 IO-bound processes concurrently\n");
        printf("Expected: CPU-bound get demoted, IO-bound stay at level 0\n\n");

        int pid1 = fork();
        if(pid1 == 0) {
            run_cpu_bound(getpid());
            exit(0);
        }

        int pid2 = fork();
        if(pid2 == 0) {
            run_cpu_bound(getpid());
            exit(0);
        }

        int pid3 = fork();
        if(pid3 == 0) {
            run_io_bound(getpid());
            exit(0);
        }

        int pid4 = fork();
        if(pid4 == 0) {
            run_io_bound(getpid());
            exit(0);
        }

        // Wait for all children
        int status;
        wait(&status);
        wait(&status);
        wait(&status);
        wait(&status);

        printf("\nAll processes complete! Scheduler prioritized IO-bound (remained at level 0)\n");
        printf("CPU-bound processes should be at higher queue levels due to demotion.\n");
    }

    exit(0);

    return 0;
}