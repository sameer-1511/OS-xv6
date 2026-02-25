#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    // Normal case
    int before = getsyscount();
    getpid();
    write(1, "x\n", 1);
    int after = getsyscount();

    printf("Parent: before=%d after=%d\n", before, after);

    // Concurrent execution + edge case
    if (fork() == 0) {
        int c_before = getsyscount();
        getpid();
        getpid();
        int c_after = getsyscount();

        printf("Child: before=%d after=%d\n", c_before, c_after);
        exit(0);
    }

    wait(0);
    exit(0);
}
