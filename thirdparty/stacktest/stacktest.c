#include <stdio.h>

int main(void) {
  /* 12KB: first 2 pages eagerly mapped, 3rd triggers lazy stack growth */
  volatile char buf[12288];

  for (int i = 0; i < 12288; i++)
    buf[i] = (char)i;

  printf("Stack growth test passed: buf[0]=%d\n", (int)buf[0]);
  return 0;
}
