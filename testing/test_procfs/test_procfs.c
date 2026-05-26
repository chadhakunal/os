/*
 * Tests for the procfs implementation.
 *
 * Exercises /proc/uptime, /proc/meminfo, /proc/<pid>/status, and directory
 * enumeration — exactly as a Linux consumer would, without any knowledge of
 * the kernel implementation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s (errno=%d)\n", name, errno); failed++; }
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Read entire file into buf (up to bufsz-1 bytes), NUL-terminate. */
static int slurp(const char *path, char *buf, size_t bufsz) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, bufsz - 1);
    close(fd);
    if (n < 0) return -1;
    buf[n] = '\0';
    return (int)n;
}

/* Return 1 if `key` appears as the first token on a line in `text`. */
static int has_key(const char *text, const char *key) {
    const char *p = text;
    size_t klen = strlen(key);
    while (p && *p) {
        if (strncmp(p, key, klen) == 0)
            return 1;
        p = strchr(p, '\n');
        if (p) p++;
    }
    return 0;
}

/* Return the numeric value after `key:` in `text`, or -1 if not found. */
static long extract_long(const char *text, const char *key) {
    const char *p = text;
    size_t klen = strlen(key);
    while (p && *p) {
        if (strncmp(p, key, klen) == 0) {
            const char *colon = strchr(p, ':');
            if (colon) return strtol(colon + 1, NULL, 10);
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* /proc itself                                                        */
/* ------------------------------------------------------------------ */

static void test_proc_dir(void) {
    DIR *d = opendir("/proc");
    result("opendir /proc succeeds", d != NULL);
    if (!d) return;

    /* Must contain at least the well-known virtual files + our own PID. */
    int found_uptime  = 0;
    int found_meminfo = 0;
    int found_pid     = 0;
    char selfpid[32];
    snprintf(selfpid, sizeof(selfpid), "%d", (int)getpid());

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, "uptime")  == 0) found_uptime  = 1;
        if (strcmp(e->d_name, "meminfo") == 0) found_meminfo = 1;
        if (strcmp(e->d_name, selfpid)   == 0) found_pid     = 1;
    }

    result("/proc contains 'uptime'",         found_uptime);
    result("/proc contains 'meminfo'",        found_meminfo);
    result("/proc contains our own PID entry", found_pid);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* /proc/uptime                                                        */
/* ------------------------------------------------------------------ */

static void test_uptime(void) {
    char buf[256];
    int n = slurp("/proc/uptime", buf, sizeof(buf));
    result("open/read /proc/uptime succeeds", n > 0);
    if (n <= 0) return;

    /* Linux format: "<uptime_seconds>.<centiseconds> <idle_seconds>.<centiseconds>\n"
     * We accept any two whitespace-separated numeric tokens >= 0. */
    double up, idle;
    int parsed = sscanf(buf, "%lf %lf", &up, &idle);
    result("uptime: two numeric fields present", parsed == 2);
    result("uptime: uptime value >= 0",          up >= 0.0);
    result("uptime: idle value >= 0",            idle >= 0.0);

    /* Must end with a newline. */
    result("uptime: ends with newline", buf[n - 1] == '\n');
}

static void test_uptime_sequential_reads(void) {
    /* Two consecutive opens should both succeed and return fresh content. */
    char buf1[256], buf2[256];
    int n1 = slurp("/proc/uptime", buf1, sizeof(buf1));
    int n2 = slurp("/proc/uptime", buf2, sizeof(buf2));
    result("uptime: two consecutive reads both succeed", n1 > 0 && n2 > 0);

    double up1, up2;
    if (sscanf(buf1, "%lf", &up1) == 1 && sscanf(buf2, "%lf", &up2) == 1)
        result("uptime: second read >= first read", up2 >= up1);
    else
        result("uptime: second read >= first read", 0);
}

/* ------------------------------------------------------------------ */
/* /proc/meminfo                                                       */
/* ------------------------------------------------------------------ */

static void test_meminfo(void) {
    char buf[1024];
    int n = slurp("/proc/meminfo", buf, sizeof(buf));
    result("open/read /proc/meminfo succeeds", n > 0);
    if (n <= 0) return;

    result("meminfo: contains 'MemTotal'",  has_key(buf, "MemTotal"));
    result("meminfo: contains 'MemFree'",   has_key(buf, "MemFree"));
    result("meminfo: contains 'MemUsed'",   has_key(buf, "MemUsed") ||
                                            has_key(buf, "MemAvailable"));

    long total = extract_long(buf, "MemTotal");
    long free  = extract_long(buf, "MemFree");

    result("meminfo: MemTotal > 0",         total > 0);
    result("meminfo: MemFree >= 0",         free >= 0);
    result("meminfo: MemFree <= MemTotal",  free <= total);

    /* Linux reports in kB. */
    result("meminfo: ends with newline", buf[n - 1] == '\n');
}

/* ------------------------------------------------------------------ */
/* /proc/<pid>/status for our own process                              */
/* ------------------------------------------------------------------ */

static void test_self_status(void) {
    pid_t pid = getpid();
    pid_t ppid = getppid();

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);

    char buf[1024];
    int n = slurp(path, buf, sizeof(buf));
    result("open/read /proc/<pid>/status succeeds", n > 0);
    if (n <= 0) return;

    result("status: contains 'Pid'",   has_key(buf, "Pid"));
    result("status: contains 'PPid'",  has_key(buf, "PPid"));
    result("status: contains 'State'", has_key(buf, "State"));

    long reported_pid  = extract_long(buf, "Pid");
    long reported_ppid = extract_long(buf, "PPid");

    result("status: Pid matches getpid()",   reported_pid  == (long)pid);
    result("status: PPid matches getppid()", reported_ppid == (long)ppid);

    result("status: ends with newline", buf[n - 1] == '\n');
}

static void test_status_state_field(void) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)getpid());

    char buf[1024];
    if (slurp(path, buf, sizeof(buf)) <= 0) {
        result("status state: read", 0);
        return;
    }

    /* Find "State:" line and check the value is a known Linux single letter. */
    const char *p = buf;
    int found = 0;
    while (p && *p) {
        if (strncmp(p, "State:", 6) == 0) {
            /* skip "State:" and whitespace */
            const char *v = p + 6;
            while (*v == ' ' || *v == '\t') v++;
            /* R=running S=sleeping D=disk-sleep T=stopped Z=zombie */
            found = (*v == 'R' || *v == 'S' || *v == 'D' ||
                     *v == 'T' || *v == 'Z');
            break;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    result("status: State value is a valid Linux state letter", found);
}

/* ------------------------------------------------------------------ */
/* /proc/<pid> directory                                               */
/* ------------------------------------------------------------------ */

static void test_pid_dir(void) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", (int)getpid());

    DIR *d = opendir(path);
    result("opendir /proc/<pid> succeeds", d != NULL);
    if (!d) return;

    int found_status = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        if (strcmp(e->d_name, "status") == 0)
            found_status = 1;

    result("/proc/<pid> contains 'status'", found_status);
    closedir(d);
}

/* ------------------------------------------------------------------ */
/* Error cases                                                         */
/* ------------------------------------------------------------------ */

static void test_nonexistent_pid(void) {
    /* PID 0 is never a valid process; /proc/0/status should fail. */
    char buf[64];
    int n = slurp("/proc/0/status", buf, sizeof(buf));
    result("/proc/0/status fails (no such process)", n < 0);

    /* A very large PID is also unlikely to exist. */
    n = slurp("/proc/999999/status", buf, sizeof(buf));
    result("/proc/999999/status fails (no such process)", n < 0);
}

static void test_readonly(void) {
    /* procfs files must not be writable. */
    int fd = open("/proc/uptime", O_WRONLY, 0);
    result("/proc/uptime is not writable (O_WRONLY fails)", fd < 0);
    if (fd >= 0) close(fd);

    fd = open("/proc/meminfo", O_RDWR, 0);
    result("/proc/meminfo is not writable (O_RDWR fails)", fd < 0);
    if (fd >= 0) close(fd);
}

static void test_stat_proc(void) {
    struct stat st;
    result("stat /proc succeeds",     stat("/proc", &st) == 0);
    result("/proc is a directory",    S_ISDIR(st.st_mode));

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", (int)getpid());
    result("stat /proc/<pid> succeeds", stat(path, &st) == 0);
    result("/proc/<pid> is a directory", S_ISDIR(st.st_mode));

    result("stat /proc/uptime succeeds",  stat("/proc/uptime",  &st) == 0);
    result("stat /proc/meminfo succeeds", stat("/proc/meminfo", &st) == 0);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("\n=== procfs Tests ===\n\n");

    printf("/proc directory:\n");
    test_proc_dir();

    printf("\n/proc/uptime:\n");
    test_uptime();
    test_uptime_sequential_reads();

    printf("\n/proc/meminfo:\n");
    test_meminfo();

    printf("\n/proc/<pid>/status:\n");
    test_self_status();
    test_status_state_field();

    printf("\n/proc/<pid> directory:\n");
    test_pid_dir();

    printf("\nError cases:\n");
    test_nonexistent_pid();
    test_readonly();

    printf("\nstat:\n");
    test_stat_proc();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
