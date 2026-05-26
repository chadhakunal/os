#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s (errno=%d)\n", name, errno); failed++; }
}

/* ------------------------------------------------------------------ */
/* 1. writev to a pipe, readv from a pipe                              */
/* ------------------------------------------------------------------ */
static void test_pipe_basic(void) {
    int pfd[2];
    if (pipe(pfd) < 0) { result("pipe basic: pipe()", 0); return; }

    const char *s1 = "hello";
    const char *s2 = ", ";
    const char *s3 = "world";

    struct iovec wv[3];
    wv[0].iov_base = (void *)s1; wv[0].iov_len = 5;
    wv[1].iov_base = (void *)s2; wv[1].iov_len = 2;
    wv[2].iov_base = (void *)s3; wv[2].iov_len = 5;

    ssize_t nw = writev(pfd[1], wv, 3);
    result("writev pipe: returns total bytes", nw == 12);

    char b1[6] = {0}, b2[3] = {0}, b3[6] = {0};
    struct iovec rv[3];
    rv[0].iov_base = b1; rv[0].iov_len = 5;
    rv[1].iov_base = b2; rv[1].iov_len = 2;
    rv[2].iov_base = b3; rv[2].iov_len = 5;

    ssize_t nr = readv(pfd[0], rv, 3);
    result("readv pipe: returns total bytes", nr == 12);
    result("readv pipe: iov[0] correct", memcmp(b1, "hello", 5) == 0);
    result("readv pipe: iov[1] correct", memcmp(b2, ", ",    2) == 0);
    result("readv pipe: iov[2] correct", memcmp(b3, "world", 5) == 0);

    close(pfd[0]);
    close(pfd[1]);
}

/* ------------------------------------------------------------------ */
/* 2. writev to a file, readv back                                     */
/* ------------------------------------------------------------------ */
static void test_file_roundtrip(void) {
    const char *path = "/test_readv_writev.tmp";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { result("file roundtrip: open write", 0); return; }

    struct iovec wv[2];
    wv[0].iov_base = (void *)"foo"; wv[0].iov_len = 3;
    wv[1].iov_base = (void *)"bar"; wv[1].iov_len = 3;

    ssize_t nw = writev(fd, wv, 2);
    close(fd);
    result("writev file: returns 6", nw == 6);

    fd = open(path, O_RDONLY, 0);
    if (fd < 0) { result("file roundtrip: open read", 0); return; }

    char a[4] = {0}, b[4] = {0};
    struct iovec rv[2];
    rv[0].iov_base = a; rv[0].iov_len = 3;
    rv[1].iov_base = b; rv[1].iov_len = 3;

    ssize_t nr = readv(fd, rv, 2);
    close(fd);
    unlink(path);

    result("readv file: returns 6",    nr == 6);
    result("readv file: iov[0] = foo", memcmp(a, "foo", 3) == 0);
    result("readv file: iov[1] = bar", memcmp(b, "bar", 3) == 0);
}

/* ------------------------------------------------------------------ */
/* 3. Empty iov entries are skipped                                    */
/* ------------------------------------------------------------------ */
static void test_zero_len_iov(void) {
    int pfd[2];
    if (pipe(pfd) < 0) { result("zero len: pipe()", 0); return; }

    char data[] = "abc";
    struct iovec wv[3];
    wv[0].iov_base = (void *)"";   wv[0].iov_len = 0;   /* skip */
    wv[1].iov_base = (void *)data; wv[1].iov_len = 3;
    wv[2].iov_base = (void *)"";   wv[2].iov_len = 0;   /* skip */

    ssize_t nw = writev(pfd[1], wv, 3);
    result("writev zero-len entries: returns 3", nw == 3);

    char buf[4] = {0};
    struct iovec rv[1];
    rv[0].iov_base = buf; rv[0].iov_len = 3;
    ssize_t nr = readv(pfd[0], rv, 1);
    result("readv after zero-len write: returns 3",   nr == 3);
    result("readv after zero-len write: data correct", memcmp(buf, "abc", 3) == 0);

    close(pfd[0]);
    close(pfd[1]);
}

/* ------------------------------------------------------------------ */
/* 4. Single iov entry (degenerate case)                               */
/* ------------------------------------------------------------------ */
static void test_single_iov(void) {
    int pfd[2];
    if (pipe(pfd) < 0) { result("single iov: pipe()", 0); return; }

    struct iovec wv = { (void *)"X", 1 };
    ssize_t nw = writev(pfd[1], &wv, 1);
    result("writev single iov: returns 1", nw == 1);

    char c = 0;
    struct iovec rv = { &c, 1 };
    ssize_t nr = readv(pfd[0], &rv, 1);
    result("readv single iov: returns 1",     nr == 1);
    result("readv single iov: value correct",  c == 'X');

    close(pfd[0]);
    close(pfd[1]);
}

/* ------------------------------------------------------------------ */
/* 5. Error: invalid fd                                                */
/* ------------------------------------------------------------------ */
static void test_bad_fd(void) {
    char buf[4];
    struct iovec rv = { buf, 4 };
    errno = 0;
    ssize_t nr = readv(-1, &rv, 1);
    result("readv bad fd: returns -1",    nr == -1);
    result("readv bad fd: errno = EBADF", errno == EBADF);

    errno = 0;
    struct iovec wv = { (void *)"x", 1 };
    ssize_t nw = writev(-1, &wv, 1);
    result("writev bad fd: returns -1",    nw == -1);
    result("writev bad fd: errno = EBADF", errno == EBADF);
}

/* ------------------------------------------------------------------ */
/* 6. Error: iovcnt = 0                                                */
/* ------------------------------------------------------------------ */
static void test_zero_iovcnt(void) {
    int pfd[2];
    if (pipe(pfd) < 0) { result("zero iovcnt: pipe()", 0); return; }

    errno = 0;
    ssize_t nr = readv(pfd[0], NULL, 0);
    result("readv iovcnt=0: returns -1",      nr == -1);
    result("readv iovcnt=0: errno = EINVAL",  errno == EINVAL);

    errno = 0;
    ssize_t nw = writev(pfd[1], NULL, 0);
    result("writev iovcnt=0: returns -1",     nw == -1);
    result("writev iovcnt=0: errno = EINVAL", errno == EINVAL);

    close(pfd[0]);
    close(pfd[1]);
}

/* ------------------------------------------------------------------ */
/* 7. writev to stdout (fd=1) — basic smoke test                       */
/* ------------------------------------------------------------------ */
static void test_writev_stdout(void) {
    struct iovec wv[2];
    wv[0].iov_base = (void *)"[writev stdout ok]\n"; wv[0].iov_len = 19;
    ssize_t nw = writev(1, wv, 1);
    result("writev stdout: returns 19", nw == 19);
}

int main(void) {
    printf("\n=== readv / writev Tests ===\n\n");

    printf("Pipe I/O:\n");
    test_pipe_basic();
    test_single_iov();
    test_zero_len_iov();

    printf("\nFile I/O:\n");
    test_file_roundtrip();

    printf("\nError handling:\n");
    test_bad_fd();
    test_zero_iovcnt();

    printf("\nSmoke tests:\n");
    test_writev_stdout();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
