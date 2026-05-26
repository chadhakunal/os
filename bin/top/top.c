#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PROCS 64
#define COLS      80

struct proc_info {
  int   pid;
  char  state;
  int   ppid;
  long  vsz_kb;   /* from statm: size * page_size / 1024 */
  long  rss_kb;   /* from statm: rss  * page_size / 1024 */
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

  /* /proc/<pid>/stat */
  snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);
  if (read_file(path, buf, sizeof(buf)) < 0) return -1;

  p->pid = atoi(pid_str);
  p->state = '?';
  p->ppid  = 0;

  /* parse: pid (comm) state ppid ... */
  char *comm_end = strrchr(buf, ')');
  if (comm_end) {
    char *s = comm_end + 1;
    while (*s == ' ') s++;
    if (*s) p->state = *s++;
    while (*s == ' ') s++;
    p->ppid = atoi(s);
  }

  /* comm from /proc/<pid>/comm */
  snprintf(path, sizeof(path), "/proc/%s/comm", pid_str);
  if (read_file(path, p->cmd, sizeof(p->cmd)) > 0) {
    /* strip trailing newline */
    int len = strlen(p->cmd);
    if (len > 0 && p->cmd[len-1] == '\n') p->cmd[len-1] = '\0';
  } else {
    strncpy(p->cmd, "?", sizeof(p->cmd));
  }

  /* /proc/<pid>/statm: size rss shared ... (in pages) */
  snprintf(path, sizeof(path), "/proc/%s/statm", pid_str);
  if (read_file(path, buf, sizeof(buf)) > 0) {
    long size_pages = atol(buf);
    char *sp = buf;
    while (*sp && *sp != ' ') sp++;
    while (*sp == ' ') sp++;
    long rss_pages = atol(sp);
    p->vsz_kb = size_pages * 4;  /* 4KB pages */
    p->rss_kb = rss_pages  * 4;
  } else {
    p->vsz_kb = 0;
    p->rss_kb = 0;
  }

  return 0;
}

static void read_meminfo(long *total_kb, long *free_kb) {
  char buf[256];
  *total_kb = 0; *free_kb = 0;
  if (read_file("/proc/meminfo", buf, sizeof(buf)) < 0) return;
  char *p = buf;
  while (*p) {
    if (strncmp(p, "MemTotal:", 9) == 0)  *total_kb = atol(p + 9);
    if (strncmp(p, "MemFree:",  8) == 0)  *free_kb  = atol(p + 8);
    while (*p && *p != '\n') p++;
    if (*p) p++;
  }
}

static void read_uptime(char *buf, int cap) {
  char tmp[64];
  if (read_file("/proc/uptime", tmp, sizeof(tmp)) < 0) {
    snprintf(buf, cap, "?");
    return;
  }
  /* format: "ticks: N\ncycles: M\n" — just grab the ticks number */
  long ticks = 0;
  char *p = strstr(tmp, "ticks: ");
  if (p) ticks = atol(p + 7);
  long secs  = ticks / 100;
  long mins  = secs / 60;
  long hours = mins / 60;
  mins %= 60;
  secs %= 60;
  if (hours > 0)
    snprintf(buf, cap, "%ldh%02ldm%02lds", hours, mins, secs);
  else
    snprintf(buf, cap, "%ldm%02lds", mins, secs);
}

static int cmp_pid(const void *a, const void *b) {
  return ((struct proc_info *)a)->pid - ((struct proc_info *)b)->pid;
}

int main(void) {
  struct proc_info procs[MAX_PROCS];

  while (1) {
    /* collect processes */
    int nprocs = 0;
    DIR *d = opendir("/proc");
    if (d) {
      struct dirent *ent;
      while ((ent = readdir(d)) != NULL && nprocs < MAX_PROCS) {
        if (is_numeric(ent->d_name)) {
          if (read_proc(ent->d_name, &procs[nprocs]) == 0)
            nprocs++;
        }
      }
      closedir(d);
    }
    qsort(procs, nprocs, sizeof(procs[0]), cmp_pid);

    long total_kb = 0, free_kb = 0;
    read_meminfo(&total_kb, &free_kb);
    long used_kb = total_kb - free_kb;

    char uptime[32];
    read_uptime(uptime, sizeof(uptime));

    /* clear screen, move cursor to top */
    write(1, "\033[2J\033[H", 7);

    /* header */
    printf("SBUnix top - up %s\n", uptime);
    printf("Tasks: %d total\n", nprocs);
    if (total_kb > 0)
      printf("Mem: %ld kB total, %ld kB used, %ld kB free\n",
             total_kb, used_kb, free_kb);
    printf("\n");
    printf("%6s %-6s %8s %8s  %s\n", "PID", "STATE", "VSZ(kB)", "RSS(kB)", "CMD");
    printf("%-*s\n", COLS - 1,
           "------  -----   --------  --------  --------------------");

    for (int i = 0; i < nprocs; i++) {
      char state_str[2] = { procs[i].state, '\0' };
      printf("%6d %-6s %8ld %8ld  %s\n",
             procs[i].pid, state_str,
             procs[i].vsz_kb, procs[i].rss_kb,
             procs[i].cmd);
    }

    fflush(stdout);
    sleep(2);
  }

  return 0;
}
