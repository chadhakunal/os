#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

/* -------------------------------------------------------------------------
 * Minimal test framework
 * ---------------------------------------------------------------------- */
static int pass_count = 0;
static int fail_count = 0;

#define CHECK(cond, name) do { \
  if (cond) { \
    printf("  [PASS] %s\n", name); \
    pass_count++; \
  } else { \
    printf("  [FAIL] %s\n", name); \
    fail_count++; \
  } \
} while (0)

/* -------------------------------------------------------------------------
 * sbfs tests
 * ---------------------------------------------------------------------- */

static void test_read_existing(void) {
  printf("\n-- Read existing file --\n");
  int fd = open("/mnt/hello.txt", O_RDONLY);
  CHECK(fd >= 0, "open /mnt/hello.txt");
  if (fd < 0) return;

  char buf[64];
  int n = read(fd, buf, sizeof(buf) - 1);
  buf[n > 0 ? n : 0] = '\0';
  CHECK(n > 0, "read returns > 0 bytes");
  CHECK(buf[0]=='H' && buf[1]=='e' && buf[2]=='l' && buf[3]=='l' && buf[4]=='o',
        "content starts with 'Hello'");
  close(fd);
}

static void test_mkdir_create_write_read(void) {
  printf("\n-- mkdir + create + write + read --\n");

  unlink("/mnt/vfstest_dir/test.txt");
  rmdir("/mnt/vfstest_dir");

  int ret = mkdir("/mnt/vfstest_dir", 0);
  CHECK(ret == 0, "mkdir /mnt/vfstest_dir");

  int fd = open("/mnt/vfstest_dir/test.txt", O_WRONLY | O_CREAT);
  CHECK(fd >= 0, "create /mnt/vfstest_dir/test.txt");
  if (fd < 0) return;

  const char *content = "Hello VFS test!";
  int written = write(fd, content, 15);
  CHECK(written == 15, "write 15 bytes");
  close(fd);

  fd = open("/mnt/vfstest_dir/test.txt", O_RDONLY);
  CHECK(fd >= 0, "re-open for read");
  if (fd < 0) return;

  char buf[32];
  int n = read(fd, buf, 15);
  buf[n > 0 ? n : 0] = '\0';
  CHECK(n == 15, "read returns 15 bytes");
  CHECK(strcmp(buf, "Hello VFS test!") == 0, "read content matches");
  close(fd);
}

static void test_readdir(void) {
  printf("\n-- readdir --\n");
  int fd = open("/mnt/vfstest_dir", O_RDONLY);
  CHECK(fd >= 0, "open /mnt/vfstest_dir");
  if (fd < 0) return;

  struct dirent dents[4];
  int n = getdents(fd, dents, 4);
  CHECK(n >= 1, "getdents returns >= 1 entry");
  if (n >= 1)
    CHECK(strcmp(dents[0].d_name, "test.txt") == 0, "dents[0].d_name == test.txt");
  close(fd);
}

static void test_unlink_rmdir(void) {
  printf("\n-- unlink + rmdir --\n");
  int ret = unlink("/mnt/vfstest_dir/test.txt");
  CHECK(ret == 0, "unlink test.txt");

  ret = rmdir("/mnt/vfstest_dir");
  CHECK(ret == 0, "rmdir vfstest_dir (empty)");
}

static void test_create_duplicate(void) {
  printf("\n-- duplicate create returns -EEXIST --\n");
  int fd1 = open("/mnt/dup.txt", O_WRONLY | O_CREAT | O_EXCL);
  int fd2 = open("/mnt/dup.txt", O_WRONLY | O_CREAT | O_EXCL);
  CHECK(fd1 >= 0, "first create succeeds");
  CHECK(fd2 < 0,  "second create fails (O_EXCL)");
  if (fd1 >= 0) close(fd1);
  if (fd2 >= 0) close(fd2);
  unlink("/mnt/dup.txt");
}

/* -------------------------------------------------------------------------
 * procfs tests
 * ---------------------------------------------------------------------- */

static void test_procfs_uptime(void) {
  printf("\n-- procfs: /proc/uptime --\n");
  int fd = open("/proc/uptime", O_RDONLY);
  CHECK(fd >= 0, "open /proc/uptime");
  if (fd < 0) return;

  char buf[128];
  int n = read(fd, buf, sizeof(buf) - 1);
  buf[n > 0 ? n : 0] = '\0';
  CHECK(n > 0, "read returns > 0 bytes");
  CHECK(buf[0]=='t' && buf[1]=='i' && buf[2]=='c' && buf[3]=='k' && buf[4]=='s',
        "content starts with 'ticks'");
  close(fd);
}

static void test_procfs_pid_status(void) {
  printf("\n-- procfs: /proc/1/status --\n");
  int fd = open("/proc/1/status", O_RDONLY);
  CHECK(fd >= 0, "open /proc/1/status");
  if (fd < 0) return;

  char buf[256];
  int n = read(fd, buf, sizeof(buf) - 1);
  buf[n > 0 ? n : 0] = '\0';
  CHECK(n > 0, "read returns > 0 bytes");
  CHECK(buf[0]=='P' && buf[1]=='i' && buf[2]=='d' && buf[3]==':',
        "content starts with 'Pid:'");
  close(fd);
}

/* -------------------------------------------------------------------------
 * Test groups
 * ---------------------------------------------------------------------- */

static void run_sbfs(void) {
  printf("\n===== sbfs =====\n");
  test_read_existing();
  test_mkdir_create_write_read();
  test_readdir();
  test_unlink_rmdir();
  test_create_duplicate();
}

static void run_procfs(void) {
  printf("\n===== procfs =====\n");
  test_procfs_uptime();
  test_procfs_pid_status();
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

static void usage(const char *prog) {
  printf("Usage: %s [--sbfs] [--procfs] [--all]\n", prog);
  printf("  --sbfs    run sbfs / VFS tests\n");
  printf("  --procfs  run procfs tests\n");
  printf("  --all     run all tests (default when no flags given)\n");
}

int main(int argc, char **argv) {
  int do_sbfs  = 0;
  int do_procfs = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--sbfs")  == 0) { do_sbfs   = 1; continue; }
    if (strcmp(argv[i], "--procfs") == 0) { do_procfs = 1; continue; }
    if (strcmp(argv[i], "--all")   == 0) { do_sbfs = do_procfs = 1; continue; }
    if (strcmp(argv[i], "--help")  == 0 ||
        strcmp(argv[i], "-h")      == 0) { usage(argv[0]); return 0; }
    printf("test-kernel: unknown flag '%s'\n", argv[i]);
    usage(argv[0]);
    return 1;
  }

  /* Default: run everything. */
  if (!do_sbfs && !do_procfs)
    do_sbfs = do_procfs = 1;

  printf("\n========== test-kernel ==========\n");

  if (do_sbfs)   run_sbfs();
  if (do_procfs) run_procfs();

  printf("\n========== RESULTS: %d passed, %d failed ==========\n\n",
         pass_count, fail_count);

  return fail_count > 0 ? 1 : 0;
}
