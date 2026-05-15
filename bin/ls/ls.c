#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : ".";

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("ls: cannot open %s\n", path);
    return 1;
  }

  struct dirent buf[32];
  int n;
  while ((n = getdents(fd, buf, 32)) > 0) {
    for (int i = 0; i < n; i++) {
      printf("%s\n", buf[i].d_name);
    }
  }

  close(fd);
  return 0;
}
