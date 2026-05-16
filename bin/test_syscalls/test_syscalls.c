#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

int test_passed = 0;
int test_failed = 0;

void test_result(const char *name, int passed) {
  if (passed) {
    printf("  [PASS] %s\n", name);
    test_passed++;
  } else {
    printf("  [FAIL] %s\n", name);
    test_failed++;
  }
}

void test_getpid() {
  pid_t pid = getpid();
  test_result("getpid returns positive value", pid > 0);
}

void test_fork_exit_status() {
  pid_t pid = fork();
  if (pid == 0) {
    // Child process - exit with specific code
    exit(42);
  } else if (pid > 0) {
    // Parent process - check we got valid child pid
    test_result("fork returns positive pid in parent", pid > 0);

    int status;
    pid_t waited = wait(&status);
    test_result("wait returns child pid", waited == pid);
    test_result("child exit status is 42", status == 42);
  } else {
    test_result("fork", 0);
  }
}

void test_read_file() {
  // Test reading an existing file
  int fd = open("/etc/rc", O_RDONLY);
  test_result("open existing file", fd >= 0);

  if (fd >= 0) {
    char buffer[16];
    ssize_t nread = read(fd, buffer, sizeof(buffer));
    test_result("read from file", nread > 0);

    // Check if it starts with shebang
    int is_script = (nread >= 2 && buffer[0] == '#' && buffer[1] == '!');
    test_result("file starts with shebang", is_script);

    close(fd);
  }
}

void test_hardlinks() {
  const char *orig  = "/hl_orig.txt";
  const char *hard1 = "/hl_link1.txt";
  const char *hard2 = "/hl_link2.txt";
  const char *msg   = "hardlink-data";
  int msg_len       = strlen(msg);

  /* Clean up any leftovers from a previous run. */
  unlink(orig);
  unlink(hard1);
  unlink(hard2);

  /* Create the original file and write content. */
  int fd = open(orig, O_WRONLY | O_CREAT, 0);
  test_result("hardlink: create original file", fd >= 0);
  if (fd < 0) return;
  write(fd, msg, msg_len);
  close(fd);

  /* Create two hardlinks. */
  test_result("hardlink: link() succeeds", link(orig, hard1) == 0);
  test_result("hardlink: second link() succeeds", link(orig, hard2) == 0);

  /* Linking to the same name again should fail (EEXIST). */
  test_result("hardlink: link() to existing name fails", link(orig, hard1) < 0);

  /* All three names should read the same content. */
  char buf[64];
  int rd;

  fd = open(hard1, O_RDONLY, 0);
  test_result("hardlink: open first link", fd >= 0);
  if (fd >= 0) {
    rd = read(fd, buf, sizeof(buf) - 1);
    buf[rd > 0 ? rd : 0] = '\0';
    close(fd);
    test_result("hardlink: first link reads same content", strcmp(buf, msg) == 0);
  }

  fd = open(hard2, O_RDONLY, 0);
  test_result("hardlink: open second link", fd >= 0);
  if (fd >= 0) {
    rd = read(fd, buf, sizeof(buf) - 1);
    buf[rd > 0 ? rd : 0] = '\0';
    close(fd);
    test_result("hardlink: second link reads same content", strcmp(buf, msg) == 0);
  }

  /* Overwrite via one link — the change must be visible through the other. */
  const char *new_msg = "updated";
  fd = open(hard1, O_WRONLY | O_TRUNC, 0);
  test_result("hardlink: open first link for writing", fd >= 0);
  if (fd >= 0) {
    write(fd, new_msg, strlen(new_msg));
    close(fd);

    fd = open(orig, O_RDONLY, 0);
    test_result("hardlink: open original after write via link", fd >= 0);
    if (fd >= 0) {
      rd = read(fd, buf, sizeof(buf) - 1);
      buf[rd > 0 ? rd : 0] = '\0';
      close(fd);
      test_result("hardlink: write via link is visible in original", strcmp(buf, new_msg) == 0);
    }
  }

  /* Unlink one name — the others must still be accessible. */
  test_result("hardlink: unlink original succeeds", unlink(orig) == 0);

  fd = open(hard1, O_RDONLY, 0);
  test_result("hardlink: first link still accessible after original unlinked", fd >= 0);
  if (fd >= 0) close(fd);

  fd = open(hard2, O_RDONLY, 0);
  test_result("hardlink: second link still accessible after original unlinked", fd >= 0);
  if (fd >= 0) close(fd);

  /* Hardlinks to directories must be rejected. */
  test_result("hardlink: link() to directory is rejected", link("/etc", "/etc_link") < 0);

  /* Hardlink to a non-existent source must fail. */
  test_result("hardlink: link() of missing source fails", link("/no_such_file", "/orphan") < 0);

  /* linkat() with AT_FDCWD should behave like link(). */
  test_result("hardlink: linkat(AT_FDCWD) succeeds",
              linkat(AT_FDCWD, hard2, AT_FDCWD, orig, 0) == 0);
  if (unlink(orig) == 0) { /* clean up the re-created orig */ }

  /* Cleanup. */
  unlink(hard1);
  unlink(hard2);
}

void test_symlinks() {
  const char *orig     = "/sl_orig.txt";
  const char *sl_abs   = "/sl_abs.txt";     /* absolute symlink → orig */
  const char *sl_rel   = "/sl_rel.txt";     /* relative symlink → sl_orig.txt */
  const char *sl_dir   = "/sl_etc";         /* symlink → /etc directory */
  const char *sl_dang  = "/sl_dang.txt";    /* dangling symlink */
  const char *sl_loop1 = "/sl_loop1.txt";
  const char *sl_loop2 = "/sl_loop2.txt";
  const char *msg      = "symlink-data";
  int msg_len          = strlen(msg);

  /* Clean up any leftovers. */
  unlink(orig); unlink(sl_abs); unlink(sl_rel);
  unlink(sl_dir); unlink(sl_dang);
  unlink(sl_loop1); unlink(sl_loop2);

  /* Create original file. */
  int fd = open(orig, O_WRONLY | O_CREAT, 0);
  test_result("symlink: create original file", fd >= 0);
  if (fd >= 0) { write(fd, msg, msg_len); close(fd); }

  /* symlink() creates a symlink inode. */
  test_result("symlink: symlink() absolute succeeds", symlink(orig, sl_abs) == 0);
  test_result("symlink: symlink() relative succeeds", symlink("sl_orig.txt", sl_rel) == 0);

  /* Opening via symlink transparently follows it. */
  char buf[64];
  int rd;

  fd = open(sl_abs, O_RDONLY, 0);
  test_result("symlink: open absolute symlink", fd >= 0);
  if (fd >= 0) {
    rd = read(fd, buf, sizeof(buf) - 1);
    buf[rd > 0 ? rd : 0] = '\0';
    close(fd);
    test_result("symlink: absolute symlink reads correct data", strcmp(buf, msg) == 0);
  }

  fd = open(sl_rel, O_RDONLY, 0);
  test_result("symlink: open relative symlink", fd >= 0);
  if (fd >= 0) {
    rd = read(fd, buf, sizeof(buf) - 1);
    buf[rd > 0 ? rd : 0] = '\0';
    close(fd);
    test_result("symlink: relative symlink reads correct data", strcmp(buf, msg) == 0);
  }

  /* Write via symlink is visible through the original. */
  const char *new_msg = "updated-via-symlink";
  fd = open(sl_abs, O_WRONLY | O_TRUNC, 0);
  test_result("symlink: open abs symlink for write", fd >= 0);
  if (fd >= 0) {
    write(fd, new_msg, strlen(new_msg));
    close(fd);
    fd = open(orig, O_RDONLY, 0);
    test_result("symlink: open original after write via symlink", fd >= 0);
    if (fd >= 0) {
      rd = read(fd, buf, sizeof(buf) - 1);
      buf[rd > 0 ? rd : 0] = '\0';
      close(fd);
      test_result("symlink: write via symlink visible in original", strcmp(buf, new_msg) == 0);
    }
  }

  /* readlink() returns the target string without following. */
  char link_target[256];
  rd = readlink(sl_abs, link_target, sizeof(link_target) - 1);
  test_result("symlink: readlink() succeeds", rd > 0);
  if (rd > 0) {
    link_target[rd] = '\0';
    test_result("symlink: readlink() returns correct target", strcmp(link_target, orig) == 0);
  }

  /* Symlink to a directory — open and traverse it. */
  test_result("symlink: symlink() to directory succeeds", symlink("/etc", sl_dir) == 0);
  int dir_fd = open(sl_dir, O_RDONLY, 0);
  test_result("symlink: open directory via symlink", dir_fd >= 0);
  if (dir_fd >= 0) close(dir_fd);

  /* symlinkat() with AT_FDCWD behaves like symlink(). */
  unlink(sl_abs);
  test_result("symlink: symlinkat(AT_FDCWD) succeeds",
              symlinkat(orig, AT_FDCWD, sl_abs) == 0);

  /* Dangling symlink — target does not exist. */
  test_result("symlink: create dangling symlink", symlink("/no_such_file", sl_dang) == 0);
  fd = open(sl_dang, O_RDONLY, 0);
  test_result("symlink: open dangling symlink fails", fd < 0);
  if (fd >= 0) close(fd);

  /* readlink() on dangling symlink still works (reads the stored path). */
  rd = readlink(sl_dang, link_target, sizeof(link_target) - 1);
  test_result("symlink: readlink() on dangling symlink succeeds", rd > 0);

  /* Symlink cycle — ELOOP expected. */
  symlink(sl_loop2, sl_loop1);
  symlink(sl_loop1, sl_loop2);
  fd = open(sl_loop1, O_RDONLY, 0);
  test_result("symlink: cyclic symlink returns error", fd < 0);
  if (fd >= 0) close(fd);

  /* Cleanup. */
  unlink(orig); unlink(sl_abs); unlink(sl_rel);
  unlink(sl_dir); unlink(sl_dang);
  unlink(sl_loop1); unlink(sl_loop2);
}

void test_chmod_stat() {
  const char *path = "/chmod_test.txt";
  const char *msg  = "permission test";

  unlink(path);

  /* Create a file with default permissions. */
  int fd = open(path, O_WRONLY | O_CREAT, 0);
  test_result("chmod/stat: create file", fd >= 0);
  if (fd < 0) return;
  write(fd, msg, strlen(msg));
  close(fd);

  /* stat() should return a regular-file mode. */
  struct stat st;
  int ret = stat(path, &st);
  test_result("chmod/stat: stat() succeeds", ret == 0);
  test_result("chmod/stat: st_ino is non-zero", st.st_ino > 0);
  test_result("chmod/stat: st_size matches written data", st.st_size == (uint64_t)strlen(msg));
  test_result("chmod/stat: default mode has write bit",
              (st.st_mode & 0200) != 0);

  /* chmod 444 — read-only for owner. */
  ret = chmod(path, 0444);
  test_result("chmod/stat: chmod(0444) succeeds", ret == 0);

  /* stat should now reflect the new permission bits. */
  ret = stat(path, &st);
  test_result("chmod/stat: stat() after chmod succeeds", ret == 0);
  test_result("chmod/stat: mode has read bit set",   (st.st_mode & 0400) != 0);
  test_result("chmod/stat: mode has write bit clear", (st.st_mode & 0200) == 0);

  /* Opening for write must be denied. */
  fd = open(path, O_WRONLY, 0);
  test_result("chmod/stat: open for write denied after chmod(0444)", fd < 0);
  if (fd >= 0) close(fd);

  /* Reading is still allowed. */
  fd = open(path, O_RDONLY, 0);
  test_result("chmod/stat: open for read allowed after chmod(0444)", fd >= 0);
  if (fd >= 0) {
    char buf[64];
    int rd = read(fd, buf, sizeof(buf) - 1);
    buf[rd > 0 ? rd : 0] = '\0';
    close(fd);
    test_result("chmod/stat: content readable after chmod(0444)", strcmp(buf, msg) == 0);
  }

  /* Restore write permission and verify writing works again. */
  test_result("chmod/stat: chmod(0666) succeeds", chmod(path, 0666) == 0);
  fd = open(path, O_WRONLY | O_TRUNC, 0);
  test_result("chmod/stat: open for write allowed after chmod(0666)", fd >= 0);
  if (fd >= 0) close(fd);

  /* chmod a non-existent path must fail. */
  test_result("chmod/stat: chmod on missing file fails", chmod("/no_such_file_xyz", 0644) < 0);

  /* stat a non-existent path must fail. */
  ret = stat("/no_such_file_xyz", &st);
  test_result("chmod/stat: stat on missing file fails", ret < 0);

  /* Cleanup. */
  unlink(path);
}

void test_chdir_getcwd() {
  char cwd1[256], cwd2[256];

  getcwd(cwd1, sizeof(cwd1));
  test_result("getcwd in initial directory", cwd1[0] == '/');

  int ret = chdir("/etc");
  test_result("chdir to /etc", ret == 0);

  getcwd(cwd2, sizeof(cwd2));
  test_result("getcwd after chdir is /etc", strcmp(cwd2, "/etc") == 0);

  // Go back
  chdir(cwd1);
  getcwd(cwd2, sizeof(cwd2));
  test_result("chdir back to original", strcmp(cwd2, cwd1) == 0);
}

void test_execve() {
  pid_t pid = fork();
  if (pid == 0) {
    // Use ls as it's an actual binary
    char *argv[] = {"/bin/ls", NULL};
    char *envp[] = {NULL};
    execve("/bin/ls", argv, envp);
    // Should not reach here if execve succeeds
    printf("  [FAIL] execve did not replace process\n");
    exit(1);
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("execve replaces process", status == 0);
  } else {
    test_result("fork for execve test", 0);
  }
}

void test_getppid() {
  pid_t parent_pid = getppid();
  test_result("getppid returns positive value", parent_pid > 0);

  pid_t my_pid = getpid();
  test_result("getppid != getpid", parent_pid != my_pid);
}

int main(int argc, char **argv) {
  printf("\n=== Kernel Syscall Tests ===\n\n");

  printf("Process Management:\n");
  test_getpid();
  test_getppid();
  test_fork_exit_status();
  test_execve();

  printf("\nFile System:\n");
  test_read_file();
  test_chdir_getcwd();
  test_hardlinks();
  test_symlinks();
  test_chmod_stat();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
