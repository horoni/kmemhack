#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "../kernel/src/kmemhack.h"

int main(int argc, char **argv)
{
  struct __memory_t mem_req;
  int val = 10000;

  /* open ioctl device */
  int fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  /* SET_MEM test */
  mem_req.pid  = atoi(argv[1]);
  mem_req.addr = (unsigned long)strtoul(argv[2], NULL, 16);
  mem_req.buf  = (char *)&val;
  mem_req.n    = 4;

  printf("Writing number: %d...\n", val);
  if (ioctl(fd, SET_MEM, &mem_req) < 0) {
    perror("ioctl");
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
