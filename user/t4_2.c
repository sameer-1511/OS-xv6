#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPAGES     200          // pages per child  (less than before — N children × NPAGES total)
#define N_CHILDREN  2    // concurrent child processes per round

// Snapshot of the stats we care about, taken before and after a run.
struct snap {
    uint64 disk_reads;
    uint64 disk_writes;
    uint64 total_latency;
    int    page_faults;
    int    swapped_out;
    int    swapped_in;
};

static void take_snap(struct snap *s)
{
    struct vmstats st;
    struct diskstats ds;
    getvmstats(getpid(), &st);
    getdiskstats(&ds);
    s->disk_reads    = ds.disk_reads;
    s->disk_writes   = ds.disk_writes;
    s->total_latency = ds.avg_disk_latency;
    s->page_faults   = st.page_faults;
    s->swapped_out   = st.pages_swapped_out;
    s->swapped_in    = st.pages_swapped_in;
}

// One child: allocate NPAGES, touch them in a scattered pattern, read back,
// write delta stats to the pipe, then exit.
static void child_work(int write_fd, int child_id)
{
    struct snap before, after;
    take_snap(&before);

    char *base = sbrk(NPAGES * 4096);
    if (base == (char *)-1) {
        printf("child %d: sbrk failed\n", child_id);
        exit(1);
    }

    // Scattered write: even pages first, then odd.
    // Adjacent children operate on different virtual regions but their swap
    // blocks are spread across the disk, so concurrent children generate
    // requests at widely separated block numbers — exactly the workload
    // where SSTF beats FCFS.
    for (int i = 0; i < NPAGES; i += 2)
        base[i * 4096] = (char)(child_id * 10 + i);
    for (int i = 1; i < NPAGES; i += 2)
        base[i * 4096] = (char)(child_id * 10 + i);

    // Read back (forces swap-ins for any evicted pages).
    int chk = 0;
    for (int i = 0; i < NPAGES; i++)
        chk += (unsigned char)base[i * 4096];

    sbrk(-(NPAGES * 4096));

    take_snap(&after);

    // Report deltas to parent via the pipe.
    // Pack as: reads  writes  latency  faults  swapped_out  swapped_in  chk
    uint64 buf[7];
    buf[0] = after.disk_reads    - before.disk_reads;
    buf[1] = after.disk_writes   - before.disk_writes;
    buf[2] = after.total_latency - before.total_latency;
    buf[3] = (uint64)(after.page_faults  - before.page_faults);
    buf[4] = (uint64)(after.swapped_out  - before.swapped_out);
    buf[5] = (uint64)(after.swapped_in   - before.swapped_in);
    buf[6] = (uint64)chk;
    write(write_fd, buf, sizeof(buf));
    close(write_fd);
    exit(0);
}

// Run one round: fork N_CHILDREN children, collect their delta stats,
// print a summary and return the total latency across all children.
uint64 run_test(int policy, const char *name)
{
    setdisksched(policy);

    int pipefd[N_CHILDREN][2];
    for (int c = 0; c < N_CHILDREN; c++) {
        printf("[parent] forking child %d for %s test...\n", c, name);
        if (pipe(pipefd[c]) < 0) {
            printf("pipe failed\n");
            exit(1);
        }
        int pid = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit(1);
        }
        if (pid == 0) {
            // Child: close all read ends + other children's write ends,
            // then do the work.
            close(pipefd[c][0]);
            // close sibling write ends already opened
            for (int j = 0; j < c; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }
            child_work(pipefd[c][1], c);
            // child_work calls exit(), never returns
        }
        // Parent: close the write end for this child.
        close(pipefd[c][1]);
    }

    // Collect results from all children.
    uint64 total_reads = 0, total_writes = 0, total_latency = 0;
    uint64 total_faults = 0, total_swout = 0, total_swin = 0;
    uint64 total_chk = 0;

    for (int c = 0; c < N_CHILDREN; c++) {
        uint64 buf[7];
        int n = read(pipefd[c][0], buf, sizeof(buf));
        close(pipefd[c][0]);
        if (n != sizeof(buf)) {
            printf("child %d pipe read failed (got %d)\n", c, n);
            continue;
        }
        total_reads   += buf[0];
        total_writes  += buf[1];
        total_latency += buf[2];
        total_faults  += buf[3];
        total_swout   += buf[4];
        total_swin    += buf[5];
        total_chk     += buf[6];
    
        wait(0);
    }
    printf("[%s] faults=%lu swapped_out=%lu swapped_in=%lu\n",
           name, total_faults, total_swout, total_swin);
    printf("     reads=%lu writes=%lu latency=%lu checksum=%lu\n",
           total_reads, total_writes, total_latency, total_chk);

    return total_latency;
}

int main(void)
{
    printf("=== Disk scheduling: FCFS vs SSTF (%d concurrent children, %d pages each) ===\n\n",
           N_CHILDREN, NPAGES);

    uint64 lat_fcfs = run_test(0, "FCFS");
    printf("\n");
    uint64 lat_sstf = run_test(1, "SSTF");

    printf("\n--- Result ---\n");
    printf("FCFS latency : %lu\n", lat_fcfs);
    printf("SSTF latency : %lu\n", lat_sstf);

    if (lat_sstf <= lat_fcfs)
        printf("PASS: SSTF <= FCFS (saved %lu latency units)\n",
               lat_fcfs - lat_sstf);
    else
        printf("FAIL: SSTF (%lu) > FCFS (%lu)\n", lat_sstf, lat_fcfs);

    exit(0);
}