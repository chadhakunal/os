#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

static int write_read_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return -1;
  write(fd, content, strlen(content));
  close(fd);

  char buf[128] = {0};
  fd = open(path, O_RDONLY);
  if (fd < 0) return -1;
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n < 0) return -1;
  return strcmp(buf, content) == 0 ? 0 : -1;
}

int main(void) {
  printf("=== mount/umount tests ===\n");

  /* 1. mount sbfs at /mnt (umount first in case rc already mounted it) */
  printf("Test: mount\n");
  umount("/mnt"); /* ignore error — may not be mounted yet */
  result("mount sbfs at /mnt", mount("", "/mnt", "sbfs", 0, NULL) == 0);

  /* 2. write a file on the mounted fs */
  printf("Test: write on /mnt\n");
  result("create and read back file on /mnt",
         write_read_file("/mnt/tmp/test_mount_check", "hello mount\n") == 0);

  /* 3. mkdir on /mnt */
  printf("Test: mkdir on /mnt\n");
  mkdir("/mnt/tmp/test_mount_dir", 0755);
  struct stat st;
  result("mkdir creates dir on /mnt",
         stat("/mnt/tmp/test_mount_dir", &st) == 0 && S_ISDIR(st.st_mode));

  /* 4. umount */
  printf("Test: umount\n");
  result("umount /mnt succeeds", umount("/mnt") == 0);

  /* 5. /mnt should now be empty (stub dir, no sbfs) */
  printf("Test: /mnt empty after umount\n");
  result("/mnt/tmp not accessible after umount",
         stat("/mnt/tmp/test_mount_check", &st) != 0);

  /* 6. remount */
  printf("Test: remount\n");
  result("remount sbfs at /mnt", mount("", "/mnt", "sbfs", 0, NULL) == 0);

  /* 7. file persists after remount */
  printf("Test: persistence after remount\n");
  int fd = open("/mnt/tmp/test_mount_check", O_RDONLY);
  result("file visible again after remount", fd >= 0);
  if (fd >= 0) close(fd);

  /* cleanup */
  unlink("/mnt/tmp/test_mount_check");
  rmdir("/mnt/tmp/test_mount_dir");

  /* leave /mnt mounted for subsequent tests */

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
