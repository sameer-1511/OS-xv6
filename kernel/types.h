typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef uint64 pde_t;

struct mlfqinfo {
  int level;     // current queue level
  int ticks[4];  // total ticks consumed at each level
  int times_scheduled; // number of times the process has been scheduled
  int total_syscalls; // total system calls made
};

struct frame {
  int in_use; // whether the frame is currently in use
  struct proc *owner; // which process owns this frame
  uint64 va; // virtual address mapped to this frame
  int ref_bits; // reference bits for page replacement
};

struct vmstats {
  int page_faults;
  int pages_evicted;
  int pages_swapped_in;
  int pages_swapped_out;
  int resident_pages;
};

struct diskstats {
  uint64 disk_reads;
  uint64 disk_writes;
  uint64 avg_disk_latency; 
};
