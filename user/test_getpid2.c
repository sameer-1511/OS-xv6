#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int pid1, pid2;

    // Normal execution
    pid1 = getpid();
    pid2 = getpid2();
    printf("Parent: getpid()=%d getpid2()=%d\n", pid1, pid2);

    // Edge case: repeated calls
    printf("(repeated call) Parent getpid2(): %d\n", getpid2());

    // Concurrent execution
    if (fork() == 0) {
        int cpid1 = getpid();
        int cpid2 = getpid2();
        printf("Child: getpid()=%d getpid2()=%d\n", cpid1, cpid2);
        exit(0);
    }

    wait(0);
    exit(0);
}
