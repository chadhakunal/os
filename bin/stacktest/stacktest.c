#include <stdio.h>

int main(void) {
  char buf[49152]; /* 3 pages: first 2 eager, 3rd triggers lazy page fault */

  for (int i = 0; i < 49152; i++)
    buf[i] = (char)i;
  for (int i = 0; i < 49152; i++) {
    if (i % 254 == 0) printf("buf[%d] = %d\n", i,buf[i]);
  }
  printf("Stack test passed: buf[0]=%d\n", buf[0]);
  return 0;
}
