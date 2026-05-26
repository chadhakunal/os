#include <stdio.h>
#include <dirent.h>

int main(void) {
  DIR *d = opendir("/proc");
  if (!d) {
    fprintf(stderr, "top: cannot open /proc\n");
    return 1;
  }
  fprintf(stderr, "top: /proc opened ok\n");
  struct dirent *ent;
  int n = 0;
  while ((ent = readdir(d)) != NULL) {
    fprintf(stderr, "  entry: %s\n", ent->d_name);
    if (++n > 20) break;
  }
  closedir(d);
  return 0;
}
