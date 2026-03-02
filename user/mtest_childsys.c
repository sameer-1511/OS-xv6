#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int pid1;

    printf("Parent syscall count before fork: %d\n", getsyscount());
    // First child
    pid1 = fork();
    if (pid1 == 0) {
        getpid();
        getpid();
        exit(0);
    }

    pause(20);

    // Normal case
    printf("Child %d syscall count: %d\n", pid1, getchildsyscount(pid1));
    printf("Parent syscall count after fork: %d\n", getsyscount());

    // Edge case: invalid PID
    printf("Invalid PID syscall count: %d\n", getchildsyscount(9999));

    while (wait(0) > 0);

    exit(0);
}
