# System calls aware MLFQ in xv6

## Implemented System Calls

Two new system calls were added to the xv6 kernel.

### getlevel()

Returns the current process's **MLFQ queue level (0–3)**, where:

- `0` → highest priority  
- `3` → lowest priority  

Implemented by adding new field qlevel in **struct proc** and updating it.
`myproc()->qlevel`


---

### getmlfqinfo(int pid, struct mlfqinfo *info)

Fills an `mlfqinfo` structure for the given PID with:

- current queue level
- number of times scheduled
- total syscall count
- array of ticks spent at each of the four levels

Returns:

- `0` → success  
- `-1` → PID not found  

The structure is defined as:

```bash
struct mlfqinfo {
int level;
int times_scheduled;
int total_syscalls;
int ticks[4];
};
```

---

# Design Decisions

## Four-Level MLFQ with Fixed Quanta

The scheduler uses **four queues (0–3)** with fixed time quanta:

| Queue Level | Quantum (ticks) |
|-------------|-----------------|
| 0 | 2 |
| 1 | 4 |
| 2 | 8 |
| 3 | 16 |

The scheduler always selects the **highest priority non-empty queue** and scans the process table in queue-level order during each scheduling round.

---

## Condition for interactive processes (ΔS ≥ ΔT)

Instead of demoting processes purely based on CPU time, the scheduler checks whether the process behaves **interactively**.

At each timer tick two quantities are computed:

ΔS = number of syscalls issued since the last window reset  
ΔT = number of timer ticks consumed in the current window

If:
`ΔS ≥ ΔT`

the process is considered **interactive** and **is not demoted**, even if it has exhausted its quantum.

This design rewards processes that frequently perform:

- I/O operations  
- sleep calls  
- short bursts of computation  

These behaviors typically generate many syscalls relative to CPU usage.

---

## Demotion on Quantum Exhaustion

A process is demoted when it is **not interactive** and has used its entire time quantum.

Condition:
`ΔS < ΔT` and `ticks_consumed >= quantum[qlevel]`


When demotion occurs:

- the process moves to the next lower priority queue
- `ticks_consumed` is reset
- `last_syscount` is reset

This begins a new measurement window in the new queue.

---

## Window Reset Policy

The measurement window (`ticks_consumed`, `last_syscount`) resets only when:

1. the process is demoted
2. a priority boost occurs
3. an interactive process completes its quantum

Importantly, the scheduler **does not reset these values on every context switch**.

---
## Priority Boost Every 128 Ticks

To prevent starvation, the system performs a **priority boost every 128 global timer ticks**.

This is implemented inside `clockintr()`:
```bash 
if (ticks % 128 == 0)
    priority_boost();
```


`priority_boost()` performs the following actions:

- resets all non-UNUSED processes to `queue level 0`
- resets `ticks_consumed`
- resets `last_syscount`

This mechanism ensures that CPU-bound processes that have fallen to lower queues will eventually regain high priority.

---

# Experimental Results

Five test programs were used to evaluate the scheduler.

---

# t_all — Basic Syscall Correctness

```bash
$ t_all
=== getlevel / getmlfqinfo Unit Tests ===

[PASS] getlevel() = 0 (in range [0,3])
[PASS] getmlfqinfo(3) succeeded
[PASS] info.level = 0 (valid)
[PASS] ticks[0..3] = [0,0,0,0] (all >= 0)
[PASS] times_scheduled = 8 (>= 1)
[PASS] getmlfqinfo(99999) returned -1 (invalid pid)

--- Results: 6 passed, 0 failed ---
```
---

# t_cpu — CPU-bound Demotion

Three CPU-bound processes performing many times, each were spawned simultaneously.

```bash
Output :
$ t_cpu
=== CPU-Bound Test ===
Spawning 3 CPU-bound processes...
[parent] pid=4 level=0 sched=0 syscalls=0 ticks=[0,0,0,0]
[child pid=4] finished burn. Final MLFQ level: 3
[parent] pid=5 level=3 sched=27 syscalls=0 ticks=[2,4,8,12]
[child pid=5] finished burn. Final MLFQ level: 3
[parent] pid=6 level=3 sched=28 syscalls=0 ticks=[2,4,8,13]
[child pid=6] finished burn. Final MLFQ level: 3
=== CPU-Bound Test Done ===
```


The tick arrays confirm the expected behavior:

- 2 ticks in level 0
- 4 ticks in level 1
- 8 ticks in level 2

After these quanta are consumed, the processes reach **level 3**, where they accumulate the remaining CPU time.

The mismatch in info written by parent and child for pid=4, because parent queries info a bit early.

---

# t_io — Interactive Process Retention

Three syscall-heavy processes executed:
with many iterations of `getpid`, `uptime`, `getppid`

```bash
Output :
$ t_io
=== Syscall-Heavy (Interactive) Test ===
Spawning 3 syscall-heavy processes...
[parent] pid=4 level=0 sched=0 syscalls=0 ticks=[0,0,0,0]

[c
hil[dchi ldp piid=4d]= 5] dodone. nFie. Finanal lMLFQ leve MLFlQ:  le0
vel: 0
[parent] pid
[=c5 hleildve l=0pid= s6ch] edone. dF=inal MLF5 Q level: 0
syscalls=6044 ticks=[2,0,0,0]
[parent] pid=6 level=0 sched=7 syscalls=6044 ticks=[1,0,0,0]
=== Syscall-Heavy Test Done ===
```

The **ΔS ≥ ΔT rule** holds clearly.

With thousands of syscalls across only a few scheduling slices, ΔS exceeds ΔT, so no demotion occurs.

All interactive processes stay in level 0;

---

# t_mixed — Mixed Workload Behavior

This test evaluates scheduler behavior when **CPU-bound and interactive processes run simultaneously**.

The workload consists of:

- CPU-bound processes performing long computation loops
- Interactive processes issuing frequent syscalls

This scenario verifies that the scheduler can **differentiate between workloads and allocate CPU time appropriately**.

Expected behavior:

- Interactive processes remain at **higher priority queues (0–1)** due to the ΔS ≥ ΔT rule.
- CPU-bound processes gradually **demote to lower queues (2–3)** as they exhaust their quanta without sufficient syscall activity.

```bash
Output :
$ t_mixed
=== Mixed Workload Test ===
Spawning 2 CPU-bound + 2 syscall-heavy processes...

--- Scheduler Statistics ---

[SYS-heavy pid=6] level=0 sched=2 syscalls=4503 ticks=[0,0,0,0]

[SYS-heavy pid=7] level=0 sched=3 syscalls=4503 ticks=[1,0,0,0]
[CPU-bound pid=5] level=3 sched=20 syscalls=2 ticks=[2,4,8,5]

[CPU-bound pid=4] level=3 sched=20 syscalls=2 ticks=[2,4,8,5]


=== Mixed Workload Test Done ===
```

Key observations:

- Interactive processes remain in **queue level 0**, confirming that frequent syscalls prevent demotion.
- CPU-bound processes follow the expected **demotion staircase** through queues 0 → 1 → 2 → 3.
- The scheduler correctly prioritizes interactive tasks while still allowing CPU-bound tasks to make progress.

This test demonstrates that the **interactivity condition successfully separates workloads**, ensuring responsive behavior for interactive tasks without starving computational jobs.

# t_starve — Anti-Starvation Boost

One CPU-bound process and one syscall-heavy interactive process ran concurrently.

The CPU-bound process periodically sampled its own queue level.

```bash
Output :
$ t_starve
=== Starvation / Global Boost Test ===
One CPU-bound + one SYS-heavy process running together.
Global boost fires every 128 ticks.


[sys-child pid=5] done. level=0
[cpu-child pid=4] sample=24 level 3→1 (BOOST #1 observed)
[cpu-child pid=4] done. boosts_seen=1 final_level=3

=== Starvation Test Done ===
```

- The boost correctly promotes the process from **level 3** to high priority queues, confirming the **128-tick global boost** works.

Most importantly, the CPU-bound process continues to be scheduled repeatedly (`times_scheduled > 0`) and **never starves**, even when competing with a permanently high-priority interactive process.

---
**Note**: The console output can be interleaved sometimes due to parent, child writing at same time.

---

# Conclusion

The implemented scheduler demonstrates:

- correct **MLFQ scheduling behavior**
- proper **demotion of CPU-bound workloads**
- retention of **interactive processes at high priority**
- periodic **priority boosting to prevent starvation**

Experimental results confirm that the scheduler behaves as expected across different workload patterns.