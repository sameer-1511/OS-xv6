// stats_test.c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
  struct diskstats ds;

  // initial stats
  getdiskstats(&ds);
  printf("Initial: R=%lu W=%lu L=%lu\n",
         ds.disk_reads, ds.disk_writes, ds.avg_disk_latency);

  // generate disk activity
  int fd = open("testfile", O_CREATE | O_RDWR);
  for(int i = 0; i < 50; i++){
    write(fd, "hello", 5);
  }
  close(fd);

  fd = open("testfile", O_RDONLY);
  char buf[10];
  for(int i = 0; i < 50; i++){
    read(fd, buf, 5);
  }
  close(fd);

  // final stats
  getdiskstats(&ds);
  printf("After: R=%lu W=%lu L=%lu\n",
         ds.disk_reads, ds.disk_writes, ds.avg_disk_latency);

  exit(0);
}