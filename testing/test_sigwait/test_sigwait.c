#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s (errno=%d)\n", name, errno); failed++; }
}

/* ------------------------------------------------------------------ */
/* 1. sigwait: block SIGUSR1, send from child, consume via sigwait     */
/* ------------------------------------------------------------------ */
static void test_sigwait_basic(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    pid_t child = fork();
    if (child == 0) {
        struct timespec ts = {0, 50000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    int sig = 0;
    int ret = sigwait(&set, &sig);
    int status;
    waitpid(child, &status, 0);

    result("sigwait: returns 0",         ret == 0);
    result("sigwait: delivers SIGUSR1",  sig == SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 2. sigwait: signal already pending before sigwait called            */
/* ------------------------------------------------------------------ */
static void test_sigwait_already_pending(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigprocmask(SIG_BLOCK, &set, NULL);

    /* Send to self while blocked — it queues without delivering. */
    kill(getpid(), SIGUSR2);

    int sig = 0;
    int ret = sigwait(&set, &sig);

    result("sigwait already pending: returns 0",        ret == 0);
    result("sigwait already pending: delivers SIGUSR2", sig == SIGUSR2);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 3. sigwaitinfo: returns signo and fills siginfo_t                   */
/* ------------------------------------------------------------------ */
static void test_sigwaitinfo(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    pid_t child = fork();
    if (child == 0) {
        struct timespec ts = {0, 50000000};
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    siginfo_t info;
    memset(&info, 0, sizeof(info));
    int ret = sigwaitinfo(&set, &info);
    int status;
    waitpid(child, &status, 0);

    result("sigwaitinfo: returns signo",       ret == SIGUSR1);
    result("sigwaitinfo: info.si_signo set",   info.si_signo == SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 4. sigwaitinfo with NULL info — must not crash                      */
/* ------------------------------------------------------------------ */
static void test_sigwaitinfo_null_info(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    kill(getpid(), SIGUSR1);

    int ret = sigwaitinfo(&set, NULL);

    result("sigwaitinfo NULL info: returns signo", ret == SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 5. sigtimedwait: signal arrives before timeout                      */
/* ------------------------------------------------------------------ */
static void test_sigtimedwait_signal_arrives(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    pid_t child = fork();
    if (child == 0) {
        struct timespec ts = {0, 50000000}; /* 50 ms */
        nanosleep(&ts, NULL);
        kill(getppid(), SIGUSR1);
        exit(0);
    }

    struct timespec timeout = {2, 0}; /* 2 s — plenty of time */
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    int ret = sigtimedwait(&set, &info, &timeout);
    int status;
    waitpid(child, &status, 0);

    result("sigtimedwait signal arrives: returns signo",     ret == SIGUSR1);
    result("sigtimedwait signal arrives: info.si_signo set", info.si_signo == SIGUSR1);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 6. sigtimedwait: timeout expires with no signal → EAGAIN            */
/* ------------------------------------------------------------------ */
static void test_sigtimedwait_timeout(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    struct timespec timeout = {0, 200000000}; /* 200 ms */
    int ret = sigtimedwait(&set, NULL, &timeout);
    int saved_errno = errno;

    result("sigtimedwait timeout: returns -1",    ret == -1);
    result("sigtimedwait timeout: errno is EAGAIN", saved_errno == EAGAIN);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 7. sigtimedwait: zero timeout, signal pending → immediate consume   */
/* ------------------------------------------------------------------ */
static void test_sigtimedwait_zero_timeout_pending(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigprocmask(SIG_BLOCK, &set, NULL);

    kill(getpid(), SIGUSR2);

    struct timespec zero = {0, 0};
    int ret = sigtimedwait(&set, NULL, &zero);

    result("sigtimedwait zero timeout pending: returns signo", ret == SIGUSR2);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

/* ------------------------------------------------------------------ */
/* 8. sigtimedwait: zero timeout, no signal pending → EAGAIN           */
/* ------------------------------------------------------------------ */
static void test_sigtimedwait_zero_timeout_empty(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigprocmask(SIG_BLOCK, &set, NULL);

    struct timespec zero = {0, 0};
    int ret = sigtimedwait(&set, NULL, &zero);
    int saved_errno = errno;

    result("sigtimedwait zero timeout empty: returns -1",     ret == -1);
    result("sigtimedwait zero timeout empty: errno is EAGAIN", saved_errno == EAGAIN);

    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int main(void) {
    printf("\n=== sigwait / sigwaitinfo / sigtimedwait Tests ===\n\n");

    printf("sigwait:\n");
    test_sigwait_basic();
    test_sigwait_already_pending();

    printf("\nsigwaitinfo:\n");
    test_sigwaitinfo();
    test_sigwaitinfo_null_info();

    printf("\nsigtimedwait:\n");
    test_sigtimedwait_signal_arrives();
    test_sigtimedwait_timeout();
    test_sigtimedwait_zero_timeout_pending();
    test_sigtimedwait_zero_timeout_empty();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
