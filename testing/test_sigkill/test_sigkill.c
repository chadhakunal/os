#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * POSIX wait-status bit layout (same as Linux):
 *
 *   bits 6:0  — terminating signal (0 = exited normally)
 *   bit  7    — core dumped (only meaningful if bits 6:0 != 0)
 *   bits 15:8 — exit code (only meaningful if bits 6:0 == 0)
 *
 * For a signal-killed child (SIGKILL = 9):
 *   expected raw status = 0x00000009
 *   WTERMSIG  = status & 0x7f        = 9    (the signal)
 *   WIFSIGNALED = (WTERMSIG > 0)     = true
 *   WIFEXITED   = (WTERMSIG == 0)    = false
 *   WEXITSTATUS = (status >> 8) & ff = 0    (meaningless)
 *
 * For a child that called exit(0):
 *   expected raw status = 0x00000000
 *   WTERMSIG  = 0, WIFEXITED = true, WEXITSTATUS = 0
 *
 * For a child that called exit(1):
 *   expected raw status = 0x00000100
 *   WTERMSIG  = 0, WIFEXITED = true, WEXITSTATUS = 1
 */

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s\n", name); failed++; }
}

/* -----------------------------------------------------------------------
 * Test 1: SIGKILL a child blocked in sleep
 * ----------------------------------------------------------------------- */
static void test_sigkill_sleeping_child(void) {
    printf("\nTest: SIGKILL child blocked in sleep\n");

    int fds[2];
    pipe(fds); /* use a pipe so parent knows child is actually sleeping */

    pid_t child = fork();
    if (child == 0) {
        close(fds[0]);
        /* Signal parent that we're about to sleep. */
        write(fds[1], "r", 1);
        close(fds[1]);
        /* Sleep for a long time — parent will SIGKILL us. */
        struct timespec ts = { .tv_sec = 60, .tv_nsec = 0 };
        nanosleep(&ts, 0);
        /* If we reach here the kernel failed to terminate us — exit nonzero
         * so the parent can detect the fault even if status bits are wrong. */
        _exit(42);
    }

    close(fds[1]);
    /* Wait until child has entered the sleep. */
    char buf[1];
    read(fds[0], buf, 1);
    close(fds[0]);

    /* Give the child a moment to reach nanosleep. */
    struct timespec tiny = { .tv_sec = 0, .tv_nsec = 50000000 }; /* 50 ms */
    nanosleep(&tiny, 0);

    kill(child, SIGKILL);

    int status = 0;
    pid_t wpid = waitpid(child, &status, 0);

    printf("  waitpid returned pid=%d  raw status=0x%08x\n", (int)wpid, (unsigned)status);
    printf("  low 7 bits (WTERMSIG field) : 0x%02x = %d\n",
           status & 0x7f, status & 0x7f);
    printf("  bit 7 (core dump flag)      : %d\n", (status >> 7) & 1);
    printf("  bits 15:8 (exit code field) : %d\n", (status >> 8) & 0xff);
    printf("\n");
    printf("  Macro results:\n");
    printf("    WIFEXITED   = %d  (want 0)\n", WIFEXITED(status));
    printf("    WIFSIGNALED = %d  (want 1)\n", WIFSIGNALED(status));
    printf("    WTERMSIG    = %d  (want %d)\n", WTERMSIG(status), SIGKILL);
    printf("    WEXITSTATUS = %d  (meaningless when signaled)\n", WEXITSTATUS(status));
    printf("\n");
    printf("  Expected raw status: 0x%08x\n", SIGKILL);
    printf("  Got    raw status:   0x%08x\n", (unsigned)status);

    result("waitpid returned child pid",         wpid == child);
    result("WIFEXITED is false",                 !WIFEXITED(status));
    result("WIFSIGNALED is true",                WIFSIGNALED(status));
    result("WTERMSIG == SIGKILL",               WTERMSIG(status) == SIGKILL);
    result("raw status low byte == SIGKILL",     (status & 0x7f) == SIGKILL);
    result("raw status high byte == 0",          ((status >> 8) & 0xff) == 0);
}

/* -----------------------------------------------------------------------
 * Test 2: SIGKILL a child blocked reading from a pipe with no writer
 * ----------------------------------------------------------------------- */
static void test_sigkill_blocked_reader(void) {
    printf("\nTest: SIGKILL child blocked reading pipe (no writer)\n");

    int sync[2]; /* parent waits for child to be ready */
    int data[2]; /* child blocks reading this */
    pipe(sync);
    pipe(data);

    pid_t child = fork();
    if (child == 0) {
        close(sync[0]);
        close(data[1]); /* child only reads */

        /* Signal parent we're about to block. */
        write(sync[1], "r", 1);
        close(sync[1]);

        /* Block forever — no writer holds data[1]. */
        char buf[4];
        read(data[0], buf, sizeof(buf));
        _exit(42);
    }

    close(sync[1]);
    close(data[0]);
    close(data[1]);

    char buf[1];
    read(sync[0], buf, 1);
    close(sync[0]);

    struct timespec tiny = { .tv_sec = 0, .tv_nsec = 50000000 };
    nanosleep(&tiny, 0);

    kill(child, SIGKILL);

    int status = 0;
    pid_t wpid = waitpid(child, &status, 0);

    printf("  waitpid returned pid=%d  raw status=0x%08x\n", (int)wpid, (unsigned)status);
    printf("  low 7 bits (WTERMSIG field) : 0x%02x = %d\n",
           status & 0x7f, status & 0x7f);
    printf("  Macro results:\n");
    printf("    WIFEXITED   = %d  (want 0)\n", WIFEXITED(status));
    printf("    WIFSIGNALED = %d  (want 1)\n", WIFSIGNALED(status));
    printf("    WTERMSIG    = %d  (want %d)\n", WTERMSIG(status), SIGKILL);

    result("waitpid returned child pid",         wpid == child);
    result("WIFEXITED is false",                 !WIFEXITED(status));
    result("WIFSIGNALED is true",                WIFSIGNALED(status));
    result("WTERMSIG == SIGKILL",               WTERMSIG(status) == SIGKILL);
}

/* -----------------------------------------------------------------------
 * Test 3: control — child exits cleanly, verify normal-exit encoding
 * ----------------------------------------------------------------------- */
static void test_normal_exit_encoding(void) {
    printf("\nTest: control — child exits cleanly with code 7\n");

    pid_t child = fork();
    if (child == 0)
        _exit(7);

    int status = 0;
    pid_t wpid = waitpid(child, &status, 0);

    printf("  raw status=0x%08x\n", (unsigned)status);
    printf("  Expected raw status: 0x%08x\n", 7 << 8);
    printf("  Macro results:\n");
    printf("    WIFEXITED   = %d  (want 1)\n", WIFEXITED(status));
    printf("    WEXITSTATUS = %d  (want 7)\n", WEXITSTATUS(status));
    printf("    WIFSIGNALED = %d  (want 0)\n", WIFSIGNALED(status));

    result("waitpid returned child pid",       wpid == child);
    result("WIFEXITED is true",               WIFEXITED(status));
    result("WEXITSTATUS == 7",               WEXITSTATUS(status) == 7);
    result("WIFSIGNALED is false",           !WIFSIGNALED(status));
    result("raw status == exitcode << 8",    status == (7 << 8));
}

int main(void) {
    printf("=== SIGKILL termination status tests ===\n");
    printf("SIGKILL = %d\n", SIGKILL);

    test_normal_exit_encoding();
    test_sigkill_sleeping_child();
    test_sigkill_blocked_reader();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed != 0;
}
