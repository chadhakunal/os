#include <unistd.h>

int main(void) {
  char buf[256];
  for (int i = 0; i < 256; i++)
    buf[i] = (char)i;

  write(1, buf, 256);

  return 0;
}
