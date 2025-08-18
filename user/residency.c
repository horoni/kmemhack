/* Test residency of virtual memory pages after different methods */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

#include "../kernel/src/kmemhack.h"

#define vm_readv_syscall 270
#define vm_writev_syscall 271

void *align_address(void *addr)
{
  long page_size = sysconf(_SC_PAGESIZE);
  return (void *)((long)addr & ~(page_size - 1));
}

int in_phys_ram(void *addr)
{
  unsigned char vec;
  long page_size = sysconf(_SC_PAGESIZE);
  
  int result = mincore(align_address(addr), page_size, &vec);
  if (result == -1) {
    printf("mincore err\n");
    return 0;
  }
  if (!(vec & 1)) {
    return 0; // NOT Loaded in real RAM
  }
  return 1;
}

int read_vm(void *addr, void *buffer, size_t length)
{
  struct iovec local;
  struct iovec remote;

  local.iov_base = buffer;
  local.iov_len = length;
  remote.iov_base = addr;
  remote.iov_len = length;

  return syscall(vm_readv_syscall, getpid(), &local, 1, &remote, 1, 0) == length;
}

int main()
{
  struct __memory_t mem;
  void *trap;
  int fd, buf;
  long page_size = sysconf(_SC_PAGESIZE);

  /* Dereference test */
  trap = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  printf("before deref: %d\n", in_phys_ram(trap));
  buf = *((int *)trap);
  printf("after deref: %d\n", in_phys_ram(trap));
  if (munmap(trap, page_size) != 0) {
    perror("munmap");
    exit(EXIT_FAILURE);
  }

  /* vm_read test */
  trap = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  printf("before vm_read: %d\n", in_phys_ram(trap));
  read_vm(trap, &buf, 4);
  printf("after vm_read: %d\n", in_phys_ram(trap));
  if (munmap(trap, page_size) != 0) {
    perror("munmap");
    exit(EXIT_FAILURE);
  }

  /* kernel test */
  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }
  trap = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  mem.pid  = getpid();
  mem.addr = (unsigned long)trap; 
  mem.buf  = (char *)&buf;
  mem.n    = 4;
  printf("before kernel: %d\n", in_phys_ram(trap));
  if (ioctl(fd, GET_MEM, &mem) < 0) {
    perror("ioctl");
    close(fd);
    return 1;
  }
  printf("after kernel: %d\n", in_phys_ram(trap));
  if (munmap(trap, page_size) != 0) {
    perror("munmap");
    exit(EXIT_FAILURE);
  }
  close(fd);
  return 0;
}

