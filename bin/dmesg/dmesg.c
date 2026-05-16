#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
  int fd = open("/proc/kmsg", O_RDONLY);
  if (fd < 0) {
    printf("dmesg: cannot open /proc/kmsg\n");
    return 1;
  }

  char buf[512];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    write(1, buf, n);
  }

  close(fd);
  return 0;
}
