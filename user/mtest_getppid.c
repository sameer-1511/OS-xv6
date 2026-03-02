

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    printf("Parent: pid = %d\n", getpid());

    // Normal + concurrent execution
    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            pause(50*i);
            printf("Child %d: ppid = %d\n", getpid(), getppid());
            exit(0);
        }
    }

    while (wait(0) > 0);

    // Edge case: no parent
    printf("Parent calling getppid(): %d\n", getppid());

    exit(0);
}
