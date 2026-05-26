#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

static int is_numeric(const char *s) {
  if (!*s) return 0;
  while (*s) { if (!isdigit((unsigned char)*s++)) return 0; }
  return 1;
}

static void print_proc(const char *pid_str) {
  char path[64];
  char stat_buf[256];
  char cmd[64] = "?";

  /* Read /proc/<pid>/stat */
  snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);
  int fd = open(path, O_RDONLY);
  if (fd < 0) return;
  int n = read(fd, stat_buf, sizeof(stat_buf) - 1);
  close(fd);
  if (n <= 0) return;
  stat_buf[n] = '\0';

  /* Parse: pid (comm) state ppid pgrp session ... */
  long pid = -1;
  char state = '?';

  /* pid */
  char *p = stat_buf;
  pid = atol(p);

  /* comm is in parentheses */
  char *comm_start = strchr(p, '(');
  char *comm_end   = strrchr(p, ')');
  if (comm_start && comm_end && comm_end > comm_start) {
    size_t len = (size_t)(comm_end - comm_start - 1);
    if (len >= sizeof(cmd)) len = sizeof(cmd) - 1;
    memcpy(cmd, comm_start + 1, len);
    cmd[len] = '\0';
    p = comm_end + 1;
  }

  /* state */
  while (*p == ' ') p++;
  if (*p) state = *p++;

  (void)state; /* suppress unused warning if not printing */

  /* Try /proc/<pid>/cmdline for a better command name */
  snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);
  fd = open(path, O_RDONLY);
  if (fd >= 0) {
    char cbuf[128];
    int cn = read(fd, cbuf, sizeof(cbuf) - 1);
    close(fd);
    if (cn > 0) {
      cbuf[cn] = '\0';
      /* basename of first NUL-terminated arg */
      char *base = cbuf;
      for (int i = 0; i < cn; i++)
        if (cbuf[i] == '\0') { cbuf[i] = ' '; }
      cbuf[cn] = '\0';
      /* trim trailing spaces */
      for (int i = cn - 1; i >= 0 && cbuf[i] == ' '; i--)
        cbuf[i] = '\0';
      /* use just the basename of argv[0] */
      char *slash = strrchr(base, '/');
      if (slash) base = slash + 1;
      strncpy(cmd, base, sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = '\0';
    }
  }

  if (pid >= 0)
    printf("%6ld %-20s\n", pid, cmd);
}

int main(void) {
  DIR *d = opendir("/proc");
  if (!d) {
    perror("ps: /proc");
    return 1;
  }

  printf("%6s %-20s\n", "PID", "CMD");

  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    if (is_numeric(ent->d_name))
      print_proc(ent->d_name);
  }
  closedir(d);
  return 0;
}
