#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PROCS 64

struct proc_info {
  int   pid;
  char  state;
  long  vsz_kb;
  long  rss_kb;
  char  cmd[32];
};

static int is_numeric(const char *s) {
  if (!*s) return 0;
  while (*s) { if (!isdigit((unsigned char)*s++)) return 0; }
  return 1;
}

static int read_file(const char *path, char *buf, int cap) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;
  int n = read(fd, buf, cap - 1);
  close(fd);
  if (n < 0) return -1;
  buf[n] = '\0';
  return n;
}

static int read_proc(const char *pid_str, struct proc_info *p) {
  char path[64], buf[256];

  p->pid    = atoi(pid_str);
  p->state  = '?';
  p->vsz_kb = 0;
  p->rss_kb = 0;
  strncpy(p->cmd, "?", sizeof(p->cmd));

  snprintf(path, sizeof(path), "/proc/%s/comm", pid_str);
  if (read_file(path, buf, sizeof(buf)) > 0) {
    strncpy(p->cmd, buf, sizeof(p->cmd) - 1);
    p->cmd[sizeof(p->cmd) - 1] = '\0';
    int len = strlen(p->cmd);
    if (len > 0 && p->cmd[len-1] == '\n') p->cmd[len-1] = '\0';
  }

  snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);
  if (read_file(path, buf, sizeof(buf)) > 0) {
    char *end = strrchr(buf, ')');
    if (end) {
      char *s = end + 1;
      while (*s == ' ') s++;
      if (*s) p->state = *s;
    }
  }

  snprintf(path, sizeof(path), "/proc/%s/statm", pid_str);
  if (read_file(path, buf, sizeof(buf)) > 0) {
    long size_p = atol(buf);
    char *sp = buf;
    while (*sp && *sp != ' ') sp++;
    while (*sp == ' ') sp++;
    long rss_p = atol(sp);
    p->vsz_kb = size_p * 4;
    p->rss_kb = rss_p  * 4;
  }

  return 0;
}

static void read_meminfo(long *total_kb, long *free_kb, long *used_kb) {
  char buf[256];
  *total_kb = *free_kb = *used_kb = 0;
  if (read_file("/proc/meminfo", buf, sizeof(buf)) < 0) return;
  char *p = buf;
  while (*p) {
    if (strncmp(p, "MemTotal:", 9) == 0) *total_kb = atol(p + 9);
    if (strncmp(p, "MemFree:",  8) == 0) *free_kb  = atol(p + 8);
    if (strncmp(p, "MemUsed:",  8) == 0) *used_kb  = atol(p + 8);
    while (*p && *p != '\n') p++;
    if (*p) p++;
  }
}

static int cmp_pid(const void *a, const void *b) {
  return ((struct proc_info *)a)->pid - ((struct proc_info *)b)->pid;
}

int main(void) {
  struct proc_info procs[MAX_PROCS];

  /* sanity check */
  DIR *test = opendir("/proc");
  if (!test) {
    fprintf(stderr, "top: cannot open /proc\n");
    return 1;
  }
  fprintf(stderr, "top: /proc opened ok\n");

  /* count entries */
  int total = 0;
  struct dirent *ent;
  while ((ent = readdir(test)) != NULL) {
    fprintf(stderr, "  entry: %s\n", ent->d_name);
    total++;
    if (total > 20) break;
  }
  closedir(test);
  fprintf(stderr, "top: counted %d entries\n", total);
  return 0;
}
