#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : ".";

  printf("ls: opening '%s'\n", path);
  int fd = open(path, O_RDONLY);
  printf("ls: open returned fd=%d\n", fd);
  if (fd < 0) {
    printf("ls: cannot open %s\n", path);
    return 1;
  }

  struct dirent buf[32];
  int n;
  int call = 0;
  while ((n = getdents(fd, buf, 32)) > 0) {
    printf("ls: getdents call %d returned %d entries\n", call++, n);
    for (int i = 0; i < n; i++) {
      printf("%s\n", buf[i].d_name);
    }
  }
  printf("ls: getdents call %d returned %d (done)\n", call, n);

  printf("ls: closing fd=%d\n", fd);
  close(fd);
  printf("ls: done\n");
  return 0;
}
