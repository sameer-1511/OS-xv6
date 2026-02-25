# CS3523 – Programming Assignment 01
## Extending xv6 with Custom System Calls

**Course:** Operating Systems II (CS3523)  
**Platform:** xv6-riscv (MIT PDOS) on QEMU  

This assignment involves adding new system calls to the xv6 kernel and writing user-level programs to test them.

---

## 1. Overview

In this assignment, I implemented a set of new system calls in xv6 related to process information and system call accounting. For each system call, I wrote a separate user-level test program. The tests were designed to check:

- normal execution  
- edge cases (such as no children or invalid PID)  
- concurrent execution using multiple processes (`fork()`)

All tests were run on xv6 and the kernel booted and executed correctly without any panics.

---

## 2. Implemented System Calls

### Part A – Warm-up

**A1. `hello()`**  
Prints the message `Hello from the kernel!` and returns `0`.

**A2. `getpid2()`**  
Returns the PID of the calling process. This was implemented separately from the existing `getpid()` system call.

---

### Part B – Process Relationships

**B1. `getppid()`**  
Returns the PID of the parent process.

**B2. `getnumchild()`**  
Returns the number of currently alive child processes of the calling process. Zombie processes are not counted.

---

### Part C – System Call Accounting

**C2. `getsyscount()`**  
Returns the total number of system calls made by the calling process since it was created.

**C3. `getchildsyscount(int pid)`**  
Returns the system call count of the given child PID. If the PID is invalid or not a child, it returns `-1`.

---

## 3. Modified Files

### Kernel Files
- `kernel/syscall.c`
- `kernel/syscall.h`
- `kernel/sysproc.c`
- `kernel/proc.c`
- `kernel/proc.h`
- `kernel/defs.h`

### User Files
- `user/user.h`
- `user/usys.pl`
- `Makefile`
- User test programs:
  - `test_hello.c`
  - `test_getpid2.c`
  - `test_getppid.c`
  - `test_numchild.c`
  - `test_syscount.c`
  - `test_csyscount.c`

---

## 4. Testing and Results

### A1 – `test_hello.c`

This program tests the `hello()` system call. The parent and multiple child processes call `hello()` concurrently. The kernel message is printed correctly each time.

Sample output:
```bash
$ test_hello
Hello from the kernel!
hello() returned 0

Hello from the kernel!
Hello from the kernel!
CHello from the kernel!
CChildh hiill5d:  h6:eldlo  (h)e4l: l o(her)eturnedl l0
 ro() eturretned u0
rned 0
```
Due to concurrent execution, the print is not readable.

---

### A2 – `test_getpid2.c`

This test compares `getpid2()` with `getpid()` in both parent and child processes to ensure correctness.

Sample output:
```bash
$ test_getpid2
Parent: getpid()=7 getpid2()=7
(repeated call) Parent getpid2(): 7
Child: getpid()=8 getpid2()=8
```



---

### B1 – `test_getppid.c`

This test checks the parent-child relationship. Multiple children call `getppid()` concurrently.

Sample output:
```bash
$ test_getppid
Parent: pid = 9
Child 10: ppid = 9
Child 11: ppid = 9
Parent calling getppid(): 2
```



Note: If the process doesn't have a parent, it returns -1

---

### B2 – `test_numchild.c`

This program creates multiple child processes and checks the number of alive children before and after they exit.

Sample output:
```bash
$ test_numchild
Number of children: 3
Number of children after wait: 0
```


---

### C2 – `test_syscount.c`

This test checks whether the system call counter increases after making system calls. Parent and child processes maintain separate counters.

Sample output:
```bash
$ test_syscount
xParent: before=3 after=6
Child: before=1 after=4
```



---

### C3 – `test_csyscount.c`

This test checks the system call count of child processes and also verifies the behavior for an invalid PID.

Sample output:
```bash
$ test_csyscount
Child 19 syscall count: 3
Child 20 syscall count: 4
Invalid PID syscall count: -1
```



---

## 5. Notes

- Output from concurrent processes may appear interleaved because xv6 console writes are not atomic.
- This does not affect the correctness of the system calls.

---

## 6. Conclusion

All required system calls were implemented and tested successfully. The test programs cover normal cases, edge cases, and concurrent execution. The xv6 kernel runs all tests correctly without any crashes.
