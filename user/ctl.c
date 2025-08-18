#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../kernel/src/kmemhack.h"

int main(int argc, char **argv) {
  struct __maps_t req;
  struct __memory_t mem_req;
  char a[] = {1, 2};

  /* open ioctl device */
  int fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  /* GET_MAPS test */
  req.pid   = atoi(argv[1]);
  req.start = 0;
  req.end   = 0;
  req.flags = 0;
  strcpy(req.name, "libc.so");
  if (ioctl(fd, GET_MAPS, &req) < 0) {
    perror("ioctl");
    close(fd);
    return 1;
  }
  printf("s:%lx e:%lx r:%d w:%d x:%d\n", req.start, req.end,
    req.flags & PROT_READ, req.flags & PROT_WRITE,
    req.flags & PROT_EXEC);

  /* GET_MEM test */
  mem_req.pid  = getpid();
  mem_req.addr = (unsigned long)a;
  mem_req.buf  = malloc(2);
  mem_req.n    = 2;

  printf("R buffer before: %d %d\n", mem_req.buf[0], mem_req.buf[1]);
  if (ioctl(fd, GET_MEM, &mem_req) < 0) {
    perror("ioctl");
    close(fd);
    return 1;
  }
  printf("R buffer after: %d %d\n", mem_req.buf[0], mem_req.buf[1]);

  /* SET_MEM test */
  mem_req.pid    = getpid();
  mem_req.addr   = (unsigned long)a;
  mem_req.buf[0] = 0; mem_req.buf[1] = 200;
  mem_req.n      = 2;

  printf("W numbers before: %d %d\n", a[0], a[1]);
  printf("W writing numbers: %d %d ...\n", mem_req.buf[0],
    mem_req.buf[1]);
  if (ioctl(fd, SET_MEM, &mem_req) < 0) {
    perror("ioctl");
    close(fd);
    return 1;
  }
  printf("W numbers after: %d %d\n", a[0], a[1]);

  /* Exit */
  free(mem_req.buf);
  close(fd);
  return 0;
}

