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

  /* Try raw write first to isolate dprintf vs pipe */
  fprintf(stderr, "step 3: calling write directly\n");
  ssize_t wr = write(fds[1], "hello", 5);
  fprintf(stderr, "step 4: write returned %zd\n", wr);

  fprintf(stderr, "step 5: calling read\n");
  char buf[256];
  ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
  fprintf(stderr, "step 6: read returned %zd\n", n);
  if (n > 0) { buf[n] = '\0'; fprintf(stderr, "step 7: got '%s'\n", buf); }

  /* Now try dprintf */
  fprintf(stderr, "step 8: calling dprintf\n");
  int ret = dprintf(fds[1], "hello %s %d", "world", 42);
  fprintf(stderr, "step 9: dprintf returned %d\n", ret);

  fprintf(stderr, "step 10: calling read again\n");
  n = read(fds[0], buf, sizeof(buf) - 1);
  fprintf(stderr, "step 11: read returned %zd\n", n);
  if (n > 0) { buf[n] = '\0'; fprintf(stderr, "step 12: got '%s'\n", buf); }

  close(fds[0]);
  close(fds[1]);
  return 0;
}
