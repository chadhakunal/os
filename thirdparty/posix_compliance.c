/*
 * POSIX compliance diagnostic program.
 *
 * Each section exercises a standard POSIX header and the declarations it
 * must provide. Compile this against the libc and fix every missing header,
 * type, constant, or prototype that the compiler reports.
 *
 * Build (from repo root):
 *   make -C thirdparty build/rootfs/bin/posix_compliance
 */

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <setjmp.h>
#include <limits.h>
#include <inttypes.h>

/* =========================================================================
 * 1. sys/time.h — struct timeval, gettimeofday
 * ====================================================================== */
static void test_sys_time(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0)
        perror("gettimeofday");
    else
        printf("gettimeofday: %ld.%06ld\n", (long)tv.tv_sec, (long)tv.tv_usec);

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0)
        perror("clock_gettime");
    else
        printf("clock_gettime: %ld.%09ld\n", (long)ts.tv_sec, ts.tv_nsec);

    struct timespec req = { .tv_sec = 0, .tv_nsec = 1 };
    if (nanosleep(&req, NULL) < 0)
        perror("nanosleep");
    else
        printf("nanosleep: ok\n");
}

/* =========================================================================
 * 2. sys/mman.h — mmap, munmap, msync
 * ====================================================================== */
static void test_mmap(void) {
    void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return; }
    memset(p, 0xAB, 4096);
    printf("mmap: first byte=0x%02x\n", ((unsigned char *)p)[0]);

    if (msync(p, 4096, MS_SYNC) < 0)
        perror("msync");
    else
        printf("msync: ok\n");

    munmap(p, 4096);
}

/* =========================================================================
 * 3. errno and strerror
 * ====================================================================== */
static void test_errno(void) {
    /* Trigger a known errno. */
    int fd = open("/nonexistent/path", O_RDONLY, 0);
    if (fd < 0)
        printf("errno: open failed, errno=%d strerror=%s\n",
               errno, strerror(errno));

    /* strsignal */
    printf("strsignal(SIGSEGV): %s\n", strsignal(SIGSEGV));
}

/* =========================================================================
 * 4. stdio FILE streams — fopen, fwrite, fread, fseek, ftell, fclose
 * ====================================================================== */
static void test_stdio(void) {
    FILE *f = tmpfile();
    if (!f) { perror("tmpfile"); return; }

    const char data[] = "hello posix";
    size_t n = fwrite(data, 1, sizeof(data) - 1, f);
    if (n != sizeof(data) - 1) { perror("fwrite"); fclose(f); return; }

    if (fseek(f, 0, SEEK_SET) < 0) { perror("fseek"); fclose(f); return; }

    char buf[32] = {0};
    n = fread(buf, 1, sizeof(buf) - 1, f);
    if (n == 0) { perror("fread"); fclose(f); return; }

    long pos = ftell(f);
    printf("stdio: fwrite/fread='%s' ftell=%ld\n", buf, pos);

    rewind(f);
    char line[32];
    if (fgets(line, sizeof(line), f))
        printf("stdio: fgets='%s'\n", line);

    fclose(f);
}

/* =========================================================================
 * 5. pipe + fork + exec — the canonical Unix pipeline
 * ====================================================================== */
static void test_pipe_exec(void) {
    int fds[2];
    if (pipe(fds) < 0) { perror("pipe"); return; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* child: write a message into the pipe and exit */
        close(fds[0]);
        const char msg[] = "pipe_msg";
        write(fds[1], msg, sizeof(msg) - 1);
        close(fds[1]);
        _exit(0);
    }

    /* parent: read from the pipe */
    close(fds[1]);
    char buf[32] = {0};
    ssize_t nr = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);

    int status;
    waitpid(pid, &status, 0);

    if (nr > 0)
        printf("pipe+fork: read '%s', child exit=%d\n",
               buf, WEXITSTATUS(status));
    else
        perror("pipe read");
}

/* =========================================================================
 * 6. string.h — strerror, strdup, strtol, strtoul, strsep
 * ====================================================================== */
static void test_strings(void) {
    /* strerror */
    printf("strerror(ENOENT): %s\n", strerror(ENOENT));

    /* strdup */
    char *s = strdup("hello");
    if (!s) { perror("strdup"); return; }
    printf("strdup: '%s'\n", s);
    free(s);

    /* strtol */
    char *end;
    long v = strtol("  -42rest", &end, 10);
    printf("strtol: %ld remainder='%s'\n", v, end);

    /* strtoul hex */
    unsigned long u = strtoul("0xff", &end, 0);
    printf("strtoul(0xff): %lu\n", u);

    /* strcasecmp */
    printf("strcasecmp(FOO,foo): %d\n", strcasecmp("FOO", "foo"));
}

/* =========================================================================
 * 7. getenv / setenv / unsetenv
 * ====================================================================== */
static void test_env(void) {
    if (setenv("POSIX_TEST_VAR", "hello", 1) < 0) {
        perror("setenv"); return;
    }
    const char *v = getenv("POSIX_TEST_VAR");
    printf("getenv: POSIX_TEST_VAR=%s\n", v ? v : "(null)");

    unsetenv("POSIX_TEST_VAR");
    v = getenv("POSIX_TEST_VAR");
    printf("unsetenv: POSIX_TEST_VAR=%s\n", v ? v : "(null)");
}

/* =========================================================================
 * 8. fcntl — O_CREAT, F_GETFL, F_SETFL, O_APPEND
 * ====================================================================== */
static void test_fcntl(void) {
    /* create a temp file */
    char path[] = "/tmp/posix_fcntl_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); return; }
    unlink(path);

    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) { perror("F_GETFL"); close(fd); return; }
    printf("fcntl F_GETFL: flags=0x%x\n", flags);

    if (fcntl(fd, F_SETFL, flags | O_APPEND) < 0)
        perror("F_SETFL O_APPEND");
    else
        printf("fcntl F_SETFL O_APPEND: ok\n");

    /* dup2: redirect fd to a known slot */
    int fd2 = dup(fd);
    if (fd2 < 0) { perror("dup"); close(fd); return; }
    printf("dup: %d -> %d\n", fd, fd2);
    close(fd2);
    close(fd);
}

/* =========================================================================
 * 9. lseek, pread, pwrite
 * ====================================================================== */
static void test_positional_io(void) {
    char path[] = "/tmp/posix_pio_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); return; }
    unlink(path);

    /* write two bytes at specific offsets via pwrite */
    if (pwrite(fd, "A", 1, 0) < 1) { perror("pwrite 0"); close(fd); return; }
    if (pwrite(fd, "B", 1, 1) < 1) { perror("pwrite 1"); close(fd); return; }

    /* pread at offset 1 — file position must not move */
    char c = 0;
    if (pread(fd, &c, 1, 1) < 1) { perror("pread"); close(fd); return; }
    printf("pread at offset 1: '%c'\n", c);

    /* lseek to beginning and read sequentially */
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); close(fd); return; }
    char buf[4] = {0};
    read(fd, buf, 2);
    printf("lseek+read: '%s'\n", buf);

    close(fd);
}

/* =========================================================================
 * 10. symlink / readlink / lstat
 * ====================================================================== */
static void test_symlink(void) {
    char target[] = "/tmp/posix_sym_target_XXXXXX";
    int fd = mkstemp(target);
    if (fd < 0) { perror("mkstemp target"); return; }
    close(fd);

    char link[] = "/tmp/posix_sym_link_XXXXXX";
    /* make link path unique by appending _lnk */
    link[sizeof(link) - 5] = 'l';
    link[sizeof(link) - 4] = 'n';
    link[sizeof(link) - 3] = 'k';
    link[sizeof(link) - 2] = '\0';

    unlink(link);
    if (symlink(target, link) < 0) { perror("symlink"); unlink(target); return; }

    char buf[256] = {0};
    ssize_t n = readlink(link, buf, sizeof(buf) - 1);
    if (n < 0)
        perror("readlink");
    else
        printf("symlink: readlink='%s'\n", buf);

    /* lstat should see a symlink */
    struct stat st;
    if (lstat(link, &st) < 0)
        perror("lstat");
    else
        printf("lstat: S_ISLNK=%d\n", S_ISLNK(st.st_mode));

    unlink(link);
    unlink(target);
}

/* =========================================================================
 * 11. setjmp / longjmp
 * ====================================================================== */
static jmp_buf jmp_env;

static void do_longjmp(void) {
    longjmp(jmp_env, 42);
}

static void test_setjmp(void) {
    int val = setjmp(jmp_env);
    if (val == 0) {
        do_longjmp();
        printf("setjmp: ERROR — should not reach here\n");
    } else {
        printf("setjmp/longjmp: returned %d\n", val);
    }
}

/* =========================================================================
 * 12. sys/resource.h — getrlimit / setrlimit
 * ====================================================================== */
static void test_rlimit(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) < 0) {
        perror("getrlimit RLIMIT_STACK"); return;
    }
    printf("getrlimit RLIMIT_STACK: cur=%llu max=%llu\n",
           (unsigned long long)rl.rlim_cur,
           (unsigned long long)rl.rlim_max);

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("getrlimit RLIMIT_NOFILE"); return;
    }
    printf("getrlimit RLIMIT_NOFILE: cur=%llu\n",
           (unsigned long long)rl.rlim_cur);
}

/* =========================================================================
 * 13. dirent — opendir, readdir, closedir
 * ====================================================================== */
static void test_dirent(void) {
    DIR *dir = opendir("/");
    if (!dir) { perror("opendir"); return; }
    struct dirent *ent;
    int count = 0;
    printf("opendir(\"/\"):");
    while ((ent = readdir(dir)) != NULL && count++ < 5)
        printf(" %s", ent->d_name);
    printf("\n");
    closedir(dir);
}

/* =========================================================================
 * 14. signal delivery — raise, sigaction, sigprocmask
 * ====================================================================== */
static volatile sig_atomic_t sig_received;

static void sig_handler(int s) { sig_received = s; }

static void test_signals(void) {
    struct sigaction sa = { .sa_handler = sig_handler };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) < 0) { perror("sigaction"); return; }

    sig_received = 0;
    raise(SIGUSR1);
    printf("raise SIGUSR1: received=%d\n", (int)sig_received);

    /* block, raise, check pending, unblock */
    sigset_t set, old, pending;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigprocmask(SIG_BLOCK, &set, &old);

    sig_received = 0;
    struct sigaction sa2 = { .sa_handler = sig_handler };
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGUSR2, &sa2, NULL);
    raise(SIGUSR2);

    sigpending(&pending);
    printf("sigpending SIGUSR2: %d\n", sigismember(&pending, SIGUSR2));

    sigprocmask(SIG_SETMASK, &old, NULL);
    printf("after unblock SIGUSR2: received=%d\n", (int)sig_received);
}

/* =========================================================================
 * 15. stat, lstat, chmod, mkdir, rmdir, rename
 * ====================================================================== */
static void test_filesystem(void) {
    /* mkdir + stat */
    const char *dir = "/tmp/posix_fstest";
    rmdir(dir);
    if (mkdir(dir, 0755) < 0) { perror("mkdir"); return; }

    struct stat st;
    if (stat(dir, &st) < 0) { perror("stat"); rmdir(dir); return; }
    printf("mkdir+stat: ino=%llu S_ISDIR=%d mode=0%03o\n",
           (unsigned long long)st.st_ino,
           S_ISDIR(st.st_mode),
           st.st_mode & 0777);

    /* create a file inside */
    char path[64];
    snprintf(path, sizeof(path), "%s/file", dir);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); rmdir(dir); return; }
    write(fd, "x", 1);
    close(fd);

    /* fstat */
    fd = open(path, O_RDONLY, 0);
    if (fstat(fd, &st) < 0) perror("fstat");
    else printf("fstat: size=%llu\n", (unsigned long long)st.st_size);
    close(fd);

    /* chmod */
    if (chmod(path, 0600) < 0) perror("chmod");
    else printf("chmod 0600: ok\n");

    /* rename */
    char path2[64];
    snprintf(path2, sizeof(path2), "%s/file2", dir);
    if (rename(path, path2) < 0) perror("rename");
    else printf("rename: ok\n");

    /* unlink + rmdir */
    unlink(path2);
    rmdir(dir);
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void) {
    printf("=== posix_compliance diagnostic ===\n\n");

    printf("-- 1. sys/time.h --\n");       test_sys_time();
    printf("\n-- 2. sys/mman.h --\n");      test_mmap();
    printf("\n-- 3. errno/strerror --\n");  test_errno();
    printf("\n-- 4. stdio streams --\n");   test_stdio();
    printf("\n-- 5. pipe+fork --\n");       test_pipe_exec();
    printf("\n-- 6. string.h --\n");        test_strings();
    printf("\n-- 7. env --\n");             test_env();
    printf("\n-- 8. fcntl --\n");           test_fcntl();
    printf("\n-- 9. pread/pwrite --\n");    test_positional_io();
    printf("\n-- 10. symlink --\n");        test_symlink();
    printf("\n-- 11. setjmp --\n");         test_setjmp();
    printf("\n-- 12. rlimit --\n");         test_rlimit();
    printf("\n-- 13. dirent --\n");         test_dirent();
    printf("\n-- 14. signals --\n");        test_signals();
    printf("\n-- 15. filesystem --\n");     test_filesystem();

    printf("\n=== done ===\n");
    return 0;
}
