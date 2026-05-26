#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_PROCS   128
#define TERM_ROWS   24
#define REFRESH_SEC 3

struct proc_info {
    int   pid;
    int   ppid;
    char  state;
    char  comm[64];
    long  vmsize_kb;   /* from /proc/<pid>/statm field 0 * 4 */
    long  vmrss_kb;    /* from /proc/<pid>/statm field 1 * 4 */
    char  cmdline[128];
};

static int is_numeric(const char *s) {
    if (!*s) return 0;
    while (*s) { if (!isdigit((unsigned char)*s++)) return 0; }
    return 1;
}

static int read_file(const char *path, char *buf, int size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, size - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = '\0';
    return n;
}

/* Parse /proc/<pid>/stat: "pid (comm) state ppid pgrp ..." */
static int parse_stat(const char *pid_str, struct proc_info *p) {
    char path[64];
    char buf[256];

    snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);
    if (read_file(path, buf, sizeof(buf)) <= 0)
        return -1;

    p->pid = atoi(pid_str);
    p->state = '?';
    p->comm[0] = '\0';

    char *comm_start = strchr(buf, '(');
    char *comm_end   = strrchr(buf, ')');
    if (comm_start && comm_end && comm_end > comm_start) {
        size_t len = (size_t)(comm_end - comm_start - 1);
        if (len >= sizeof(p->comm)) len = sizeof(p->comm) - 1;
        memcpy(p->comm, comm_start + 1, len);
        p->comm[len] = '\0';

        char *rest = comm_end + 1;
        while (*rest == ' ') rest++;
        if (*rest) p->state = *rest++;
        while (*rest == ' ') rest++;
        p->ppid = atoi(rest);
    }
    return 0;
}

/* Parse /proc/<pid>/statm: size resident shared text 0 data 0 (all in pages) */
static void parse_statm(const char *pid_str, struct proc_info *p) {
    char path[64];
    char buf[128];

    snprintf(path, sizeof(path), "/proc/%s/statm", pid_str);
    if (read_file(path, buf, sizeof(buf)) <= 0)
        return;

    char *s = buf;
    long vm_pages  = atol(s);
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;
    long rss_pages = atol(s);
    p->vmsize_kb = vm_pages * 4;
    p->vmrss_kb  = rss_pages * 4;
}

/* Read /proc/<pid>/cmdline, replacing NULs with spaces. */
static void parse_cmdline(const char *pid_str, struct proc_info *p) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        strncpy(p->cmdline, p->comm, sizeof(p->cmdline) - 1);
        p->cmdline[sizeof(p->cmdline) - 1] = '\0';
        return;
    }
    int n = read(fd, p->cmdline, sizeof(p->cmdline) - 1);
    close(fd);
    if (n <= 0) {
        strncpy(p->cmdline, p->comm, sizeof(p->cmdline) - 1);
        p->cmdline[sizeof(p->cmdline) - 1] = '\0';
        return;
    }
    p->cmdline[n] = '\0';
    for (int i = 0; i < n; i++)
        if (p->cmdline[i] == '\0') p->cmdline[i] = ' ';
    /* trim trailing spaces */
    for (int i = n - 1; i >= 0 && p->cmdline[i] == ' '; i--)
        p->cmdline[i] = '\0';
}

/* Parse /proc/meminfo: MemTotal, MemFree, MemUsed in kB. */
static void parse_meminfo(long *total_kb, long *free_kb, long *used_kb) {
    char buf[256];
    *total_kb = *free_kb = *used_kb = 0;
    if (read_file("/proc/meminfo", buf, sizeof(buf)) <= 0)
        return;

    char *p = buf;
    while (*p) {
        if (strncmp(p, "MemTotal:", 9) == 0)
            *total_kb = atol(p + 9);
        else if (strncmp(p, "MemFree:", 8) == 0)
            *free_kb = atol(p + 8);
        else if (strncmp(p, "MemUsed:", 8) == 0)
            *used_kb = atol(p + 8);
        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

/* Parse /proc/uptime: ticks and cycles. */
static void parse_uptime(long *ticks) {
    char buf[128];
    *ticks = 0;
    if (read_file("/proc/uptime", buf, sizeof(buf)) <= 0)
        return;
    /* "ticks: N\ncycles: M\n" */
    char *p = strstr(buf, "ticks:");
    if (p) *ticks = atol(p + 6);
}


static void collect_procs(struct proc_info *procs, int *count) {
    *count = 0;
    DIR *d = opendir("/proc");
    if (!d) { write(2, "collect: opendir fail\n", 22); return; }

    struct dirent *ent;
    int total = 0;
    while ((ent = readdir(d)) != NULL && *count < MAX_PROCS) {
        total++;
        int num = is_numeric(ent->d_name);
        write(2, "ent: ", 5); write(2, ent->d_name, strlen(ent->d_name));
        write(2, num ? " NUM\n" : " skip\n", num ? 5 : 6);
        if (!num) continue;
        struct proc_info *p = &procs[*count];
        memset(p, 0, sizeof(*p));
        char dbgpath[64]; char dbgbuf[256];
        snprintf(dbgpath, sizeof(dbgpath), "/proc/%s/stat", ent->d_name);
        int dbgfd = open(dbgpath, O_RDONLY);
        write(2, "  open stat: fd=", 16);
        char dbgn[4]; dbgn[0] = '0' + (dbgfd < 0 ? 0 : dbgfd); dbgn[1] = '\n'; dbgn[2] = 0;
        write(2, dbgn, 2);
        if (dbgfd >= 0) {
            int dbgr = read(dbgfd, dbgbuf, sizeof(dbgbuf)-1);
            dbgbuf[dbgr < 0 ? 0 : dbgr] = '\0';
            write(2, "  content: [", 12); write(2, dbgbuf, strlen(dbgbuf)); write(2, "]\n", 2);
            close(dbgfd);
        }
        if (parse_stat(ent->d_name, p) < 0) {
            write(2, "  stat fail\n", 12); continue;
        }
        parse_statm(ent->d_name, p);
        parse_cmdline(ent->d_name, p);
        (*count)++;
        write(2, "  count++\n", 10);
    }
    closedir(d);
    write(2, "collect: done, count=", 21);
    char cn[4]; cn[0] = '0' + *count; cn[1] = '\n'; write(2, cn, 2);
}

static void render(struct proc_info *procs, int count) {
    long total_kb, free_kb, used_kb, ticks;
    parse_meminfo(&total_kb, &free_kb, &used_kb);
    parse_uptime(&ticks);

    /* ~100 ticks/sec at 10MHz TIMER_INTERVAL_CYCLES=100000 */
    long uptime_sec = ticks / 100;
    long up_h  = uptime_sec / 3600;
    long up_m  = (uptime_sec % 3600) / 60;
    long up_s  = uptime_sec % 60;

    printf("--- top - up %ld:%02ld:%02ld  tasks: %d ---\n", up_h, up_m, up_s, count);
    printf("MiB Mem: %6ld.0 total  %6ld.0 free  %6ld.0 used\n",
           total_kb / 1024, free_kb / 1024, used_kb / 1024);
    printf("\n");

    /* Header line — match Linux top column order */
    printf("  PID  PPID S  VIRT  RES COMMAND\n");

    /* Reserve 4 lines for header; list up to remaining rows */
    int max_rows = TERM_ROWS - 4;
    write(2, "render: entering loop\n", 22);
    for (int i = 0; i < count && i < max_rows; i++) {
        struct proc_info *p = &procs[i];
        write(2, "render: row\n", 12);
        printf("%5d %5d %c %5ldm %4ldm %-s\n",
               p->pid, p->ppid, p->state,
               p->vmsize_kb / 1024,
               p->vmrss_kb  / 1024,
               p->cmdline[0] ? p->cmdline : p->comm);
        write(2, "render: row done\n", 17);
    }
    write(2, "render: loop done\n", 18);
    fflush(stdout);
}

int main(void) {
    static struct proc_info procs[MAX_PROCS];
    int count;

    while (1) {
        collect_procs(procs, &count);
        write(2, "main: after collect\n", 20);
        render(procs, count);
        write(2, "main: after render\n", 19);
        struct timespec ts = { REFRESH_SEC, 0 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
