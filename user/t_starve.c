// starvation.c
// Tests that low-priority processes are NOT starved.
// One high-priority syscall-heavy process runs alongside
// a CPU-bound process that would otherwise sink to Level 3.
// After 128 ticks the global boost fires and the CPU-bound
// process returns to Level 0, preventing starvation

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SAMPLE_INTERVAL 50     // iterations between level samples
#define TOTAL_SAMPLES   60     // total samples taken by cpu child
#define SYS_ROUNDS      5000   // syscall-heavy work

// CPU-bound child: sample its own level periodically
static void cpu_child(void) {
    int prev_level = getlevel();
    int boosts_seen = 0;

    for (int s = 0; s < TOTAL_SAMPLES; s++) {
        // Burn a bit
        volatile long x = 0;
        for (long i = 0; i < 100000000; i++) x += i;
        (void)x;

        int cur = getlevel();

        // Detect a boost: level decreased or jumped back to 0
        if (prev_level > cur) {
            boosts_seen++;
            printf("[cpu-child pid=%d] sample=%d level %d→%d (BOOST #%d observed)\n",
                   getpid(), s, prev_level, cur, boosts_seen);
        }
        prev_level = cur;
    }

    printf("[cpu-child pid=%d] done. boosts_seen=%d final_level=%d\n",
           getpid(), boosts_seen, getlevel());
    exit(0);
}

// Syscall-heavy child: keeps priority high
static void sys_child(void) {
    for (int r = 0; r < SYS_ROUNDS; r++) {
        getpid(); uptime(); getppid();
    }
    write(1, "\n", 1);
    printf("[sys-child pid=%d] done. level=%d\n", getpid(), getlevel());
    exit(0);
}

int main(void) {
    printf("=== Starvation / Global Boost Test ===\n");
    printf("One CPU-bound + one SYS-heavy process running together.\n");
    printf("Global boost fires every 128 ticks.\n\n");

    int cpu_pid = fork();
    if (cpu_pid < 0) { printf("fork failed\n"); exit(1); }
    if (cpu_pid == 0) cpu_child();

    int sys_pid = fork();
    if (sys_pid < 0) { printf("fork failed\n"); exit(1); }
    if (sys_pid == 0) sys_child();

    // Parent waits for both
    for (int i = 0; i < 2; i++) {
        int pid = wait(0);
        struct mlfqinfo info;
        if (getmlfqinfo(pid, &info) == 0) {
            printf("[parent] pid=%d level=%d sched=%d syscalls=%d "
                   "ticks=[%d,%d,%d,%d]\n",
                   pid, info.level, info.times_scheduled,
                   info.total_syscalls,
                   info.ticks[0], info.ticks[1],
                   info.ticks[2], info.ticks[3]);
        }
    }

    printf("\n=== Starvation Test Done ===\n");
    printf("Expected: cpu-child reports at least 1 boost observed.\n");
    printf("          times_scheduled for cpu-child > 0 (not starved).\n");
    exit(0);
}
