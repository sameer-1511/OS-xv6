// cpubound.c
// Spawns CPU-bound child processes and checks their MLFQ levels.
// Expectation: processes should migrate from Level 0 down to Level 3
// because they never yield voluntarily and burn full time quanta.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_PROCS 3
#define BURN_ITERS 600000000   // enough to consume many ticks

static void burn_cpu(void) {
    volatile long x = 0;
    for (long i = 0; i < BURN_ITERS; i++)
        x += i;
    (void)x;
}

int main(void) {
    int pids[NUM_PROCS];

    printf("=== CPU-Bound Test ===\n");
    printf("Spawning %d CPU-bound processes...\n", NUM_PROCS);

    for (int i = 0; i < NUM_PROCS; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            // Child: burn CPU then report its own level
            burn_cpu();
            int level = getlevel();
            printf("[child pid=%d] finished burn. Final MLFQ level: %d\n",
                   getpid(), level);
            exit(0);
        }
        pids[i] = pid;
    }

    for (int i = 0; i < NUM_PROCS; i++) {
        // int pid = wait(0);
        struct mlfqinfo info;
        int ret = getmlfqinfo(pids[i], &info);
        if (ret == 0) {
            printf("[parent] pid=%d level=%d sched=%d syscalls=%d "
                   "ticks=[%d,%d,%d,%d]\n",
                   pids[i], info.level, info.times_scheduled,
                   info.total_syscalls,
                   info.ticks[0], info.ticks[1],
                   info.ticks[2], info.ticks[3]);
        } else {
            printf("[parent] pid=%d already exited (info unavailable)\n", pids[i]);
        }
        wait(0); // wait for child to exit if it hasn't already
    }

    printf("=== CPU-Bound Test Done ===\n");
    exit(0);
}
