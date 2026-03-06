#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(void) {
    printf("CPU bound process started\n");

    for(int i = 0; i < 10; i++) {
        int level = getlevel();
        printf("Iteration %d - Queue level: %d\n", i, level);

        for(int j = 0; j < 100000000; j++);
    }

    exit(0);
}

// #include "kernel/types.h"
// #include "kernel/stat.h"
// #include "user/user.h"

// int main() {
//     printf("Mixed workload test\n");

//     for(int i = 0; i < 10; i++) {

//         int level = getlevel();
//         printf("Iteration %d - Queue level: %d\n", i, level);

//         if(i % 2 == 0) {
//             // CPU heavy
//             for(volatile int j = 0; j < 40000000; j++);
//         } else {
//             // IO
//             pause(20);
//         }
//     }

//     exit(0);
// }