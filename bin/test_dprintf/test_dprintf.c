#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  fprintf(stderr, "step 1: calling pipe\n");
  int fds[2];
  if (pipe(fds) < 0) {
    fprintf(stderr, "pipe failed\n");
    return 1;
  }
  fprintf(stderr, "step 2: pipe ok, fds[0]=%d fds[1]=%d\n", fds[0], fds[1]);

  fprintf(stderr, "step 3: calling dprintf\n");
  int ret = dprintf(fds[1], "hello %s %d", "world", 42);
  fprintf(stderr, "step 4: dprintf returned %d\n", ret);

  fprintf(stderr, "step 5: calling read\n");
  char buf[256];
  ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
  fprintf(stderr, "step 6: read returned %zd\n", n);

  if (n > 0) {
    buf[n] = '\0';
    fprintf(stderr, "step 7: got '%s'\n", buf);
  }

  close(fds[0]);
  close(fds[1]);
  return 0;
}
