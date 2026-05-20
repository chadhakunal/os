#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

static int is_numeric(const char *s) {
  if (!*s) return 0;
  while (*s) { if (!isdigit((unsigned char)*s++)) return 0; }
  return 1;
}

static void print_proc(const char *pid_str) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/%s/status", pid_str);

  FILE *f = fopen(path, "r");
  if (!f) return;

  char line[256];
  long pid = -1, ppid = -1;
  char state[64] = "?";

  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "Pid:", 4) == 0)
      pid = atol(line + 4);
    else if (strncmp(line, "PPid:", 5) == 0)
      ppid = atol(line + 5);
    else if (strncmp(line, "State:", 6) == 0) {
      char *p = line + 6;
      while (*p == ' ' || *p == '\t') p++;
      size_t len = strlen(p);
      if (len && p[len-1] == '\n') p[len-1] = '\0';
      strncpy(state, p, sizeof(state) - 1);
    }
  }
  fclose(f);

  if (pid >= 0)
    printf("%6ld %6ld  %-20s\n", pid, ppid, state);
}

int main(void) {
  DIR *d = opendir("/proc");
  if (!d) {
    perror("ps: /proc");
    return 1;
  }

  printf("%6s %6s  %-20s\n", "PID", "PPID", "STATE");

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (is_numeric(ent->d_name))
      print_proc(ent->d_name);
  }
  closedir(d);
  return 0;
}
