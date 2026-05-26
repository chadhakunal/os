/*
 * SA_RESTART tests.
 *
 * Each test forks a child that sleeps briefly then sends a signal to the
 * parent.  The parent is blocked inside a syscall.  With SA_RESTART the
 * syscall must complete without the parent seeing EINTR; without SA_RESTART
 * (or with SIG_DFL) the syscall must return -1/EINTR.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s\n", name); failed++; }
}

static volatile int handler_ran = 0;

static void noop_handler(int sig) {
    (void)sig;
    handler_ran = 1;
}

/* ------------------------------------------------------------------ */
/* 1. read() on a pipe with SA_RESTART: should block until data arrives
 *    even after the signal fires.                                      */
/* ------------------------------------------------------------------ */
static void test_read_restart(void) {
    handler_ran = 0;

    int pipefd[2];
    if (pipe(pipefd) < 0) { result("read SA_RESTART", 0); return; }

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    pid_t child = fork();
    if (child == 0) {
        close(pipefd[0]);
        /* Let parent enter read(), then send signal, then write data. */
        struct timespec ts = {0, 50000000}; /* 50 ms */
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        nanosleep(&ts, NULL);
        write(pipefd[1], "X", 1);
        close(pipefd[1]);
        exit(0);
    }

    close(pipefd[1]);
    char buf[4] = {0};
    int n = read(pipefd[0], buf, 1);
    close(pipefd[0]);

    int status;
    waitpid(child, &status, 0);

    /* With SA_RESTART, read must NOT return -1/EINTR; it must return 1. */
    result("read SA_RESTART: handler ran",   handler_ran == 1);
    result("read SA_RESTART: read returned data (not EINTR)", n == 1 && buf[0] == 'X');
}

/* ------------------------------------------------------------------ */
/* 2. read() without SA_RESTART: should return -1/EINTR                */
/* ------------------------------------------------------------------ */
static void test_read_no_restart(void) {
    handler_ran = 0;

    int pipefd[2];
    if (pipe(pipefd) < 0) { result("read no-restart: EINTR", 0); return; }

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = 0;   /* no SA_RESTART */
    sigaction(SIGUSR1, &sa, NULL);

    pid_t child = fork();
    if (child == 0) {
        close(pipefd[0]);
        struct timespec ts = {0, 50000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        /* Don't write — parent should get EINTR and close the read end. */
        struct timespec ts2 = {1, 0};
        nanosleep(&ts2, NULL);
        close(pipefd[1]);
        exit(0);
    }

    close(pipefd[1]);
    char buf[4];
    int n = read(pipefd[0], buf, 1);
    int saved_errno = errno;
    close(pipefd[0]);

    /* Tell child we're done so it doesn't hang. */
    int status;
    waitpid(child, &status, 0);

    result("read no-restart: handler ran",    handler_ran == 1);
    result("read no-restart: returns EINTR",  n == -1 && saved_errno == EINTR);
}

/* ------------------------------------------------------------------ */
/* 3. nanosleep() with SA_RESTART: nanosleep is non-restartable by     */
/*    design (remaining time would be wrong).  Even with SA_RESTART it */
/*    must return -1/EINTR and write the remaining time to rem.        */
/* ------------------------------------------------------------------ */
static void test_nanosleep_restart(void) {
    handler_ran = 0;

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = SA_RESTART;  /* nanosleep ignores this */
    sigaction(SIGUSR1, &sa, NULL);

    pid_t child = fork();
    if (child == 0) {
        struct timespec ts = {0, 100000000}; /* 100 ms */
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    /* Sleep for 5 s — should be cut short by signal at ~100 ms. */
    struct timespec req = {5, 0};
    struct timespec rem = {0, 0};
    int ret = nanosleep(&req, &rem);
    int saved_errno = errno;

    int status;
    waitpid(child, &status, 0);

    result("nanosleep SA_RESTART: handler ran",          handler_ran == 1);
    result("nanosleep SA_RESTART: returns EINTR (non-restartable)", ret == -1 && saved_errno == EINTR);
    result("nanosleep SA_RESTART: rem has remaining time", rem.tv_sec > 0);
}

/* ------------------------------------------------------------------ */
/* 4. nanosleep() without SA_RESTART: must return -1/EINTR             */
/* ------------------------------------------------------------------ */
static void test_nanosleep_no_restart(void) {
    handler_ran = 0;

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = 0;
    sigaction(SIGUSR1, &sa, NULL);

    pid_t child = fork();
    if (child == 0) {
        struct timespec ts = {0, 100000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    struct timespec req = {5, 0}; /* 5 s — signal should cut it short */
    int ret = nanosleep(&req, NULL);
    int saved_errno = errno;

    int status;
    waitpid(child, &status, 0);

    result("nanosleep no-restart: handler ran",   handler_ran == 1);
    result("nanosleep no-restart: returns EINTR", ret == -1 && saved_errno == EINTR);
}

/* ------------------------------------------------------------------ */
/* 5. waitpid() with SA_RESTART: must restart and reap child           */
/* ------------------------------------------------------------------ */
static void test_waitpid_restart(void) {
    handler_ran = 0;

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);

    /* Signal-sender: pings us at ~100 ms */
    pid_t pinger = fork();
    if (pinger == 0) {
        struct timespec ts = {0, 100000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    /* Slow child: exits at ~300 ms */
    pid_t slow_child = fork();
    if (slow_child == 0) {
        struct timespec ts = {0, 300000000};
        nanosleep(&ts, NULL);
        exit(42);
    }

    int wstatus;
    pid_t reaped = waitpid(slow_child, &wstatus, 0);

    /* Reap pinger too */
    int ps;
    waitpid(pinger, &ps, 0);

    result("waitpid SA_RESTART: handler ran",             handler_ran == 1);
    result("waitpid SA_RESTART: reaped correct child",    reaped == slow_child);
    result("waitpid SA_RESTART: child exit status correct", WEXITSTATUS(wstatus) == 42);
}

/* ------------------------------------------------------------------ */
/* 6. waitpid() without SA_RESTART: must return -1/EINTR               */
/* ------------------------------------------------------------------ */
static void test_waitpid_no_restart(void) {
    handler_ran = 0;

    struct sigaction sa = {0};
    sa.sa_handler = noop_handler;
    sa.sa_flags   = 0;
    sigaction(SIGUSR1, &sa, NULL);

    /* Signal-sender */
    pid_t pinger = fork();
    if (pinger == 0) {
        struct timespec ts = {0, 100000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    /* Slow child that we never get to reap via the interrupted waitpid */
    pid_t slow_child = fork();
    if (slow_child == 0) {
        struct timespec ts = {2, 0};
        nanosleep(&ts, NULL);
        exit(0);
    }

    int wstatus;
    pid_t reaped = waitpid(slow_child, &wstatus, 0);
    int saved_errno = errno;

    /* Clean up: reap slow_child with a second waitpid, then pinger. */
    if (reaped != slow_child)
        waitpid(slow_child, &wstatus, 0);
    waitpid(pinger, &wstatus, 0);

    result("waitpid no-restart: handler ran",   handler_ran == 1);
    result("waitpid no-restart: returns EINTR", reaped == -1 && saved_errno == EINTR);
}

int main(void) {
    printf("\n=== SA_RESTART Tests ===\n\n");

    printf("read() tests:\n");
    test_read_restart();
    test_read_no_restart();

    printf("\nnanosleep() tests:\n");
    test_nanosleep_restart();
    test_nanosleep_no_restart();

    printf("\nwaitpid() tests:\n");
    test_waitpid_restart();
    test_waitpid_no_restart();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
