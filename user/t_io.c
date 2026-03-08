// syscallheavy.c
// Spawns syscall-heavy child processes and checks their MLFQ levels.
// Each child repeatedly issues system calls (getpid, uptime, write)
// so that ΔS >= ΔT during every slice → should NOT be demoted.
// Expectation: processes remain at Level 0 or Level 1.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_PROCS   3
#define ROUNDS      2000       // iterations of syscall bursts

int main(void) {
    int pids[NUM_PROCS];

    printf("=== Syscall-Heavy (Interactive) Test ===\n");
    printf("Spawning %d syscall-heavy processes...\n", NUM_PROCS);

    for (int i = 0; i < NUM_PROCS; i++) {
        int pid = fork();
        if (pid < 0) { printf("fork failed\n"); exit(1); }
        if (pid == 0) {
            // Child: issue lots of syscalls
            for (int r = 0; r < ROUNDS; r++) {
                getpid();          // syscall
                uptime();          // syscall
                getppid();         // syscall
            }
            write(1, "\n", 1);

            int level = getlevel();
            printf("[child pid=%d] done. Final MLFQ level: %d\n",
                   getpid(), level);
            exit(0);
        }
        pids[i] = pid;
    }

    for (int i = 0; i < NUM_PROCS; i++) {
        int pid = pids[i];
        struct mlfqinfo info;
        int ret = getmlfqinfo(pid, &info);
        if (ret == 0) {
            printf("[parent] pid=%d level=%d sched=%d syscalls=%d "
                   "ticks=[%d,%d,%d,%d]\n",
                   pid, info.level, info.times_scheduled,
                   info.total_syscalls,
                   info.ticks[0], info.ticks[1],
                   info.ticks[2], info.ticks[3]);
        } else {
            printf("[parent] pid=%d exited (info unavailable)\n", pid);
        }
        wait(0); // wait for child to exit if it hasn't already
    }

    printf("=== Syscall-Heavy Test Done ===\n");
    printf("Expected: processes stay at Level 0 or Level 1 due to ΔS >= ΔT rule.\n");
    exit(0);
}
