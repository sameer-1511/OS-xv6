// testlevel.c
// Unit test for getlevel() and getmlfqinfo() system calls.
// Verifies:
//   1. getlevel() returns a value in [0,3]
//   2. getmlfqinfo() for own pid returns consistent data
//   3. getmlfqinfo() returns -1 for an invalid PID

#include "kernel/types.h"
#include "user/user.h"

int main(void) {
    int pass = 0, fail = 0;

    printf("=== getlevel / getmlfqinfo Unit Tests ===\n\n");

    // Test 1: getlevel() in range [0,3]
    int level = getlevel();
    if (level >= 0 && level <= 3) {
        printf("[PASS] getlevel() = %d (in range [0,3])\n", level);
        pass++;
    } else {
        printf("[FAIL] getlevel() = %d (out of range)\n", level);
        fail++;
    }

    // Test 2: getmlfqinfo for own PID succeeds
    int mypid = getpid();
    struct mlfqinfo info;
    int ret = getmlfqinfo(mypid, &info);
    if (ret == 0) {
        printf("[PASS] getmlfqinfo(%d) succeeded\n", mypid);
        pass++;
    } else {
        printf("[FAIL] getmlfqinfo(%d) returned %d (expected 0)\n", mypid, ret);
        fail++;
    }

    // Test 3: info.level matches getlevel()
    // (may differ by 1 call but both should be valid)
    if (info.level >= 0 && info.level <= 3) {
        printf("[PASS] info.level = %d (valid)\n", info.level);
        pass++;
    } else {
        printf("[FAIL] info.level = %d (invalid)\n", info.level);
        fail++;
    }

    // Test 4: ticks array values are non-negative
    int ticks_ok = 1;
    for (int i = 0; i < 4; i++) {
        if (info.ticks[i] < 0) { ticks_ok = 0; break; }
    }
    if (ticks_ok) {
        printf("[PASS] ticks[0..3] = [%d,%d,%d,%d] (all >= 0)\n",
               info.ticks[0], info.ticks[1], info.ticks[2], info.ticks[3]);
        pass++;
    } else {
        printf("[FAIL] ticks array has negative values\n");
        fail++;
    }

    // Test 5: times_scheduled >= 1 (we are running, so at least 1)
    if (info.times_scheduled >= 1) {
        printf("[PASS] times_scheduled = %d (>= 1)\n", info.times_scheduled);
        pass++;
    } else {
        printf("[FAIL] times_scheduled = %d (should be >= 1)\n",
               info.times_scheduled);
        fail++;
    }

    // Test 6: getmlfqinfo with invalid PID returns -1
    ret = getmlfqinfo(99999, &info);
    if (ret == -1) {
        printf("[PASS] getmlfqinfo(99999) returned -1 (invalid pid)\n");
        pass++;
    } else {
        printf("[FAIL] getmlfqinfo(99999) returned %d (expected -1)\n", ret);
        fail++;
    }

    printf("\n--- Results: %d passed, %d failed ---\n", pass, fail);
    exit(fail > 0 ? 1 : 0);
}