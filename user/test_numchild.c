#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    int i;

    // Concurrent execution: create multiple children
    for (i = 0; i < 3; i++) {
        if (fork() == 0) {
            pause(50);  
            exit(0);     
        }
    }

    pause(10);  // ensure children are alive
    printf("Number of children: %d\n", getnumchild());

    // Wait for all children
    while (wait(0) > 0);

    // Edge case: no children
    printf("Number of children after wait: %d\n", getnumchild());

    exit(0);
}
