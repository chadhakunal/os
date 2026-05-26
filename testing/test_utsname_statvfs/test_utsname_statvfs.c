#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s (errno=%d)\n", name, errno); failed++; }
}

/* ------------------------------------------------------------------ */
/* uname                                                               */
/* ------------------------------------------------------------------ */
static void test_uname(void) {
    struct utsname u;
    memset(&u, 0, sizeof(u));

    int ret = uname(&u);
    result("uname: returns 0",              ret == 0);
    result("uname: sysname non-empty",      u.sysname[0] != '\0');
    result("uname: machine non-empty",      u.machine[0] != '\0');
    result("uname: release non-empty",      u.release[0] != '\0');
    result("uname: sysname = sbunix",       strcmp(u.sysname,  "sbunix")  == 0);
    result("uname: machine = riscv64",      strcmp(u.machine,  "riscv64") == 0);

    printf("    sysname=%s nodename=%s release=%s machine=%s\n",
           u.sysname, u.nodename, u.release, u.machine);
}

static void test_uname_null(void) {
    errno = 0;
    int ret = uname(NULL);
    result("uname NULL: returns -1",    ret == -1);
    result("uname NULL: errno = EFAULT", errno == EFAULT);
}

/* ------------------------------------------------------------------ */
/* statvfs                                                             */
/* ------------------------------------------------------------------ */
static void test_statvfs_root(void) {
    struct statvfs sv;
    memset(&sv, 0, sizeof(sv));

    int ret = statvfs("/", &sv);
    result("statvfs /: returns 0",         ret == 0);
    result("statvfs /: f_bsize > 0",       sv.f_bsize > 0);
    result("statvfs /: f_namemax > 0",     sv.f_namemax > 0);

    printf("    bsize=%lu blocks=%lu bfree=%lu files=%lu namemax=%lu\n",
           sv.f_bsize, (unsigned long)sv.f_blocks,
           (unsigned long)sv.f_bfree, (unsigned long)sv.f_files,
           sv.f_namemax);
}

static void test_statvfs_proc(void) {
    struct statvfs sv;
    int ret = statvfs("/proc", &sv);
    result("statvfs /proc: returns 0",   ret == 0);
    result("statvfs /proc: f_bsize > 0", ret == 0 && sv.f_bsize > 0);
}

static void test_statvfs_bad_path(void) {
    struct statvfs sv;
    errno = 0;
    int ret = statvfs("/no/such/path", &sv);
    result("statvfs bad path: returns -1",    ret == -1);
    result("statvfs bad path: errno = ENOENT", errno == ENOENT);
}

static void test_statvfs_null(void) {
    errno = 0;
    int ret = statvfs(NULL, NULL);
    result("statvfs NULL: returns -1",    ret == -1);
    result("statvfs NULL: errno = EFAULT", errno == EFAULT);
}

int main(void) {
    printf("\n=== utsname / statvfs Tests ===\n\n");

    printf("uname:\n");
    test_uname();
    test_uname_null();

    printf("\nstatvfs:\n");
    test_statvfs_root();
    test_statvfs_proc();
    test_statvfs_bad_path();
    test_statvfs_null();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
