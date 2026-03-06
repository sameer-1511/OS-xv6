#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {

    struct mlfqinfo pinfo1;
    int pid1 = getpid();

    if(getmlfqinfo(pid1, &pinfo1) < 0){
        printf("Error: getmlfqinfo failed for pid %d\n", pid1);
        exit(1);
    }
    printf("Process %d: level=%d, ticks=[%d, %d, %d, %d], times_scheduled=%d, total_syscalls=%d\n", 
            pid1, pinfo1.level, pinfo1.ticks[0], pinfo1.ticks[1], pinfo1.ticks[2], pinfo1.ticks[3], 
            pinfo1.times_scheduled, pinfo1.total_syscalls);

    for(int i=0; i<50000000;i++);

    if(getmlfqinfo(pid1, &pinfo1) < 0){
        printf("Error: getmlfqinfo failed for pid %d\n", pid1);
        exit(1);
    }
    printf("Process %d: level=%d, ticks=[%d, %d, %d, %d], times_scheduled=%d, total_syscalls=%d\n", 
            pid1, pinfo1.level, pinfo1.ticks[0], pinfo1.ticks[1], pinfo1.ticks[2], pinfo1.ticks[3], 
            pinfo1.times_scheduled, pinfo1.total_syscalls);

    return 0;
}