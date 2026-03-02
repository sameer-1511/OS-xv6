#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int i;
    int ret;

    // Normal execution
    ret = hello();
    printf("hello() returned %d\n\n", ret);

    // Concurrent execution
    for (i = 0; i < 3; i++) {
        if (fork() == 0) {
            ret = hello();
            printf("Child %d: hello() returned %d\n", getpid(), ret);
            exit(0);
        }
    }

    while (wait(0) > 0);

    exit(0);
}
