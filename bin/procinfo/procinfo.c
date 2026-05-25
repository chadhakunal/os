#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
  int fd = open("/proc/self/status", O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "procinfo: cannot open /proc/self/status\n");
    return 1;
  }
  char buf[512];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, (size_t)n);
  close(fd);
  return 0;
}
