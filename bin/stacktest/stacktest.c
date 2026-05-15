#include <stdio.h>

int main(void) {
  char buf[12288]; /* 3 pages: first 2 eager, 3rd triggers lazy page fault */

  for (int i = 0; i < 12288; i++)
    buf[i] = (char)i;

  printf("Stack test passed: buf[0]=%d\n", buf[0]);
  return 0;
}
