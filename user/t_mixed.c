// mixedload.c
// Spawns both CPU-bound and syscall-heavy processes simultaneously.
// Demonstrates that CPU-bound processes sink to lower queues while
// interactive ones stay high, and that interactive processes get
// more CPU time due to higher priority.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define CPU_PROCS    2
#define SYS_PROCS    2
#define BURN_ITERS   500000000   // large enough to reach level 3
#define SYS_ROUNDS   1500

static void burn_cpu(void) {
    volatile long x = 0;
    for (long i = 0; i < BURN_ITERS; i++) x += i;
    (void)x;
}

static void do_syscalls(void) {
    for (int r = 0; r < SYS_ROUNDS; r++) {
        getpid();
        uptime();
        getsyscount();
    }
    write(1, "\n", 1);
}

int main(void) {
    // int pids[CPU_PROCS + SYS_PROCS];
    // int types[CPU_PROCS + SYS_PROCS]; // 0=cpu, 1=sys

    printf("=== Mixed Workload Test ===\n");
    printf("Spawning %d CPU-bound + %d syscall-heavy processes...\n",
           CPU_PROCS, SYS_PROCS);

    int idx = 0;

    // Spawn CPU-bound
    for (int i = 0; i < CPU_PROCS; i++, idx++) {
        int pid = fork();
        if (pid < 0) { printf("fork failed\n"); exit(1); }
        if (pid == 0) {
            burn_cpu();
            // Child self-reports before exiting (data valid here)
            int mypid = getpid();
            struct mlfqinfo info;
            if (getmlfqinfo(mypid, &info) == 0) {
                printf("[CPU-bound pid=%d] level=%d sched=%d syscalls=%d "
                       "ticks=[%d,%d,%d,%d]\n",
                       mypid, info.level, info.times_scheduled,
                       info.total_syscalls,
                       info.ticks[0], info.ticks[1],
                       info.ticks[2], info.ticks[3]);
            } else {
                printf("[CPU-bound pid=%d] done. Level: %d\n", mypid, getlevel());
            }
            exit(0);
        }
        // pids[idx]  = pid;
        // types[idx] = 0;
    }

    // Spawn syscall-heavy
    for (int i = 0; i < SYS_PROCS; i++, idx++) {
        int pid = fork();
        if (pid < 0) { printf("fork failed\n"); exit(1); }
        if (pid == 0) {
            do_syscalls();
            // Child self-reports before exiting
            int mypid = getpid();
            struct mlfqinfo info;
            if (getmlfqinfo(mypid, &info) == 0) {
                printf("[SYS-heavy pid=%d] level=%d sched=%d syscalls=%d "
                       "ticks=[%d,%d,%d,%d]\n",
                       mypid, info.level, info.times_scheduled,
                       info.total_syscalls,
                       info.ticks[0], info.ticks[1],
                       info.ticks[2], info.ticks[3]);
            } else {
                printf("[SYS-heavy pid=%d] done. Level: %d\n", mypid, getlevel());
            }
            exit(0);
        }
        // pids[idx]  = pid;
        // types[idx] = 1;
    }

    // Wait for all children
    printf("\n--- Scheduler Statistics ---\n");
    for (int i = 0; i < CPU_PROCS + SYS_PROCS; i++) {
        wait(0);
    }

    printf("\n=== Mixed Workload Test Done ===\n");
    printf("Expected:\n");
    printf("  CPU-bound  -> Level 2 or 3, high ticks[2]/ticks[3]\n");
    printf("  SYS-heavy  -> Level 0 or 1, high ticks[0]/ticks[1]\n");
    exit(0);
}