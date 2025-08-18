#include <stdio.h>
#include <unistd.h>

int main(void)
{
  int a = 31;

  printf("my pid: %d\n", getpid());
  printf("address of a: %p\n", &a);
  for (;;) {
    a++;
    printf("a = %d\n", a);
    sleep(1);
  }
  return 0;
}
