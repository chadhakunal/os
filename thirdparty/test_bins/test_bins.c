#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

extern char **environ;

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
  if (ok) { printf("  [PASS] %s\n", name); passed++; }
  else     { printf("  [FAIL] %s\n", name); failed++; }
}

static int child_ok(int status) {
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* Run argv, capture stdout into buf (null-terminated), return exit status. */
static int run_capture(char *const argv[], char *buf, size_t bufsz) {
  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
    execve(argv[0], argv, environ);
    _exit(127);
  }
  close(fds[1]);
  size_t total = 0;
  ssize_t n;
  while (total < bufsz - 1 && (n = read(fds[0], buf + total, bufsz - 1 - total)) > 0)
    total += n;
  buf[total] = '\0';
  close(fds[0]);
  int status;
  waitpid(pid, &status, 0);
  return status;
}

/* Run argv, return exit status, discard output. */
static int run_exit(char *const argv[]) {
  char buf[4];
  return run_capture(argv, buf, sizeof(buf));
}

/* -----------------------------------------------------------------------
 * echo
 * -------------------------------------------------------------------- */
static void test_echo(void) {
  printf("Test: echo\n");
  char buf[64];

  char *argv1[] = { "/bin/echo", "hello", NULL };
  run_capture(argv1, buf, sizeof(buf));
  result("echo prints argument with newline", strcmp(buf, "hello\n") == 0);

  char *argv2[] = { "/bin/echo", "foo", "bar", NULL };
  run_capture(argv2, buf, sizeof(buf));
  result("echo joins multiple args with space", strcmp(buf, "foo bar\n") == 0);

  char *argv3[] = { "/bin/echo", "-n", "no newline", NULL };
  run_capture(argv3, buf, sizeof(buf));
  result("echo -n suppresses newline", strcmp(buf, "no newline") == 0);

  char *argv4[] = { "/bin/echo", NULL };
  run_capture(argv4, buf, sizeof(buf));
  result("echo with no args prints empty line", strcmp(buf, "\n") == 0);
}

/* -----------------------------------------------------------------------
 * true / false
 * -------------------------------------------------------------------- */
static void test_true_false(void) {
  printf("Test: true/false\n");

  char *ta[] = { "/bin/true", NULL };
  result("true exits 0", child_ok(run_exit(ta)));

  char *fa[] = { "/bin/false", NULL };
  int fs = run_exit(fa);
  result("false exits non-zero", WIFEXITED(fs) && WEXITSTATUS(fs) != 0);
}

/* -----------------------------------------------------------------------
 * pwd
 * -------------------------------------------------------------------- */
static void test_pwd(void) {
  printf("Test: pwd\n");
  char buf[256];

  char *argv[] = { "/bin/pwd", NULL };
  int status = run_capture(argv, buf, sizeof(buf));
  result("pwd exits 0", child_ok(status));

  /* strip trailing newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("pwd output starts with /", buf[0] == '/');
  result("pwd output is non-empty", strlen(buf) > 0);
}

/* -----------------------------------------------------------------------
 * wc
 * -------------------------------------------------------------------- */
static void test_wc(void) {
  printf("Test: wc\n");
  char buf[128];

  /* wc from a file: /etc/rc should produce consistent counts */
  char *argv1[] = { "/bin/wc", "/etc/rc", NULL };
  int status = run_capture(argv1, buf, sizeof(buf));
  result("wc file exits 0", child_ok(status));

  long lines, words, bytes;
  int matched = sscanf(buf, "%ld %ld %ld", &lines, &words, &bytes);
  result("wc file produces 3 counts", matched == 3);
  result("wc file line count > 0", lines > 0);
  result("wc file byte count >= line count", bytes >= lines);

  /* wc via pipe: feed known input */
  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[1]);
    dup2(fds[0], 0);
    close(fds[0]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(fds[0]);
  const char *input = "one two three\nfour five\n";
  write(fds[1], input, strlen(input));
  close(fds[1]);

  /* capture wc's stdout via a second pipe */
  int fds2[2];
  pipe(fds2);
  /* wc's stdout was inherited from parent — we need a different approach:
   * re-run wc with both stdin and stdout redirected */
  waitpid(pid, NULL, 0);
  close(fds2[0]); close(fds2[1]);

  /* Simpler: fork with both ends redirected */
  int in_fds[2], out_fds[2];
  pipe(in_fds);
  pipe(out_fds);
  pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0],  0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  write(in_fds[1], input, strlen(input));
  close(in_fds[1]);

  size_t total = 0;
  ssize_t n;
  while (total < sizeof(buf)-1 && (n = read(out_fds[0], buf+total, sizeof(buf)-1-total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);
  waitpid(pid, &status, 0);

  result("wc stdin exits 0", child_ok(status));
  matched = sscanf(buf, "%ld %ld %ld", &lines, &words, &bytes);
  result("wc stdin: 2 lines", matched == 3 && lines == 2);
  result("wc stdin: 5 words", words == 5);
  result("wc stdin: correct bytes", bytes == (long)strlen(input));

  /* wc multiple files shows total line */
  char *argv2[] = { "/bin/wc", "/etc/rc", "/etc/rc", NULL };
  run_capture(argv2, buf, sizeof(buf));
  result("wc two files output contains 'total'", strstr(buf, "total") != NULL);
}

/* -----------------------------------------------------------------------
 * kill — just verify it exits cleanly with a valid signal/pid
 * -------------------------------------------------------------------- */
static void test_kill(void) {
  printf("Test: kill\n");

  /* kill with no args should exit non-zero */
  char *argv1[] = { "/bin/kill", NULL };
  int s = run_exit(argv1);
  result("kill with no args exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* send SIGCONT to ourselves — harmless */
  char pid_str[16];
  pid_t self = getpid();
  int len = 0;
  unsigned long v = (unsigned long)self;
  if (v == 0) { pid_str[len++] = '0'; }
  else { char tmp[16]; int tl = 0; while (v) { tmp[tl++] = '0' + v%10; v/=10; } while (tl--) pid_str[len++] = tmp[tl+1]; }
  /* simpler: use sprintf */
  snprintf(pid_str, sizeof(pid_str), "%d", (int)self);

  char *argv2[] = { "/bin/kill", "-18", pid_str, NULL }; /* 18 = SIGCONT */
  s = run_exit(argv2);
  result("kill -18 self exits 0", child_ok(s));
}

/* -----------------------------------------------------------------------
 * ps
 * -------------------------------------------------------------------- */
static void test_ps(void) {
  printf("Test: ps\n");
  char buf[1024];

  char *argv[] = { "/bin/ps", NULL };
  int status = run_capture(argv, buf, sizeof(buf));
  result("ps exits 0", child_ok(status));
  result("ps output contains PID header", strstr(buf, "PID") != NULL);

  /* our own pid should appear */
  char pid_str[16];
  snprintf(pid_str, sizeof(pid_str), "%d", (int)getpid());
  result("ps output contains current PID", strstr(buf, pid_str) != NULL);
}

/* -----------------------------------------------------------------------
 * test (the binary)
 * -------------------------------------------------------------------- */
static void test_test(void) {
  printf("Test: test\n");

  char *t1[] = { "/bin/test", "-f", "/etc/rc", NULL };
  result("test -f existing file exits 0", child_ok(run_exit(t1)));

  char *t2[] = { "/bin/test", "-f", "/no/such/file", NULL };
  int s = run_exit(t2);
  result("test -f missing file exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t3[] = { "/bin/test", "-d", "/bin", NULL };
  result("test -d directory exits 0", child_ok(run_exit(t3)));

  char *t4[] = { "/bin/test", "-d", "/etc/rc", NULL };
  s = run_exit(t4);
  result("test -d on file exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t5[] = { "/bin/test", "hello", "=", "hello", NULL };
  result("test string = exits 0", child_ok(run_exit(t5)));

  char *t6[] = { "/bin/test", "foo", "=", "bar", NULL };
  s = run_exit(t6);
  result("test string != exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t7[] = { "/bin/test", "3", "-gt", "2", NULL };
  result("test 3 -gt 2 exits 0", child_ok(run_exit(t7)));

  char *t8[] = { "/bin/test", "1", "-gt", "2", NULL };
  s = run_exit(t8);
  result("test 1 -gt 2 exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  char *t9[] = { "/bin/test", "-z", "", NULL };
  result("test -z empty string exits 0", child_ok(run_exit(t9)));

  char *t10[] = { "/bin/test", "-n", "nonempty", NULL };
  result("test -n nonempty exits 0", child_ok(run_exit(t10)));

  char *t11[] = { "/bin/test", "-e", "/etc/rc", NULL };
  result("test -e existing path exits 0", child_ok(run_exit(t11)));
}

/* -----------------------------------------------------------------------
 * wc piped from echo
 * -------------------------------------------------------------------- */
static void test_wc_pipe_echo(void) {
  printf("Test: wc via pipe from echo\n");
  char buf[64];

  /* echo "a b c" | wc  =>  1 line, 3 words, 6 bytes ("a b c\n") */
  int in_fds[2], out_fds[2];
  pipe(in_fds); pipe(out_fds);

  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0],  0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *wargv[] = { "/bin/wc", NULL };
    execve("/bin/wc", wargv, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  const char *input = "a b c\n";
  write(in_fds[1], input, strlen(input));
  close(in_fds[1]);

  size_t total = 0; ssize_t n;
  while (total < sizeof(buf)-1 && (n = read(out_fds[0], buf+total, sizeof(buf)-1-total)) > 0)
    total += n;
  buf[total] = '\0';
  close(out_fds[0]);

  int status; waitpid(pid, &status, 0);
  long lines, words, bytes;
  int matched = sscanf(buf, "%ld %ld %ld", &lines, &words, &bytes);
  result("echo|wc: 1 line",    matched == 3 && lines == 1);
  result("echo|wc: 3 words",   words == 3);
  result("echo|wc: 6 bytes",   bytes == 6);
}

/* Helper: write content to a file directly via open/write. */
static int write_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return -1;
  size_t len = strlen(content);
  ssize_t n = write(fd, content, len);
  close(fd);
  return (n == (ssize_t)len) ? 0 : -1;
}

/* Helper: read file content into buf. Returns bytes read, -1 on error. */
static int read_file(const char *path, char *buf, size_t bufsz) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;
  ssize_t n = read(fd, buf, bufsz - 1);
  close(fd);
  if (n < 0) return -1;
  buf[n] = '\0';
  return (int)n;
}

/* -----------------------------------------------------------------------
 * touch, mkdir, rmdir, rm
 * -------------------------------------------------------------------- */
static void test_fs_basics(void) {
  printf("Test: touch/mkdir/rmdir/rm\n");
  char buf[64];

  /* touch creates a file */
  char *t1[] = { "/bin/touch", "/mnt/tmp/tb_touch", NULL };
  result("touch creates file", child_ok(run_exit(t1)));
  result("touch: file exists after touch", stat("/mnt/tmp/tb_touch", &(struct stat){}) == 0);

  /* rm removes it */
  char *r1[] = { "/bin/rm", "/mnt/tmp/tb_touch", NULL };
  result("rm removes file", child_ok(run_exit(r1)));
  result("rm: file gone after rm", stat("/mnt/tmp/tb_touch", &(struct stat){}) != 0);

  /* mkdir creates a dir */
  char *m1[] = { "/bin/mkdir", "/mnt/tmp/tb_dir", NULL };
  result("mkdir creates dir", child_ok(run_exit(m1)));
  struct stat st;
  result("mkdir: is a directory", stat("/mnt/tmp/tb_dir", &st) == 0 && S_ISDIR(st.st_mode));

  /* rmdir removes it */
  char *rd1[] = { "/bin/rmdir", "/mnt/tmp/tb_dir", NULL };
  result("rmdir removes dir", child_ok(run_exit(rd1)));
  result("rmdir: dir gone", stat("/mnt/tmp/tb_dir", &st) != 0);

  /* mkdir -p creates nested dirs */
  char *m2[] = { "/bin/mkdir", "-p", "/mnt/tmp/tb_nest/a/b", NULL };
  result("mkdir -p nested", child_ok(run_exit(m2)));
  result("mkdir -p: leaf is dir", stat("/mnt/tmp/tb_nest/a/b", &st) == 0 && S_ISDIR(st.st_mode));

  /* cleanup */
  char *rd2[] = { "/bin/rmdir", "/mnt/tmp/tb_nest/a/b", NULL };
  char *rd3[] = { "/bin/rmdir", "/mnt/tmp/tb_nest/a", NULL };
  char *rd4[] = { "/bin/rmdir", "/mnt/tmp/tb_nest", NULL };
  run_exit(rd2); run_exit(rd3); run_exit(rd4);
  (void)buf;
}

/* -----------------------------------------------------------------------
 * cp and cat
 * -------------------------------------------------------------------- */
static void test_cp_cat(void) {
  printf("Test: cp/cat\n");
  char buf[256];

  /* Write a known file in /tmp */
  write_file("/mnt/tmp/tb_src", "hello cp\n");

  /* cp it */
  char *cp1[] = { "/bin/cp", "/mnt/tmp/tb_src", "/mnt/tmp/tb_dst", NULL };
  result("cp exits 0", child_ok(run_exit(cp1)));

  /* cat it back */
  char *cat1[] = { "/bin/cat", "/mnt/tmp/tb_dst", NULL };
  run_capture(cat1, buf, sizeof(buf));
  result("cp: content preserved", strcmp(buf, "hello cp\n") == 0);

  /* cat from tarfs file */
  char *cat2[] = { "/bin/cat", "/etc/rc", NULL };
  int s = run_capture(cat2, buf, sizeof(buf));
  result("cat tarfs file exits 0", child_ok(s));
  result("cat tarfs file non-empty", strlen(buf) > 0);

  /* cleanup */
  unlink("/mnt/tmp/tb_src");
  unlink("/mnt/tmp/tb_dst");
}

/* -----------------------------------------------------------------------
 * mv
 * -------------------------------------------------------------------- */
static void test_mv(void) {
  printf("Test: mv\n");
  char buf[64];

  write_file("/mnt/tmp/tb_mv_src", "mv content\n");

  char *mv1[] = { "/bin/mv", "/mnt/tmp/tb_mv_src", "/mnt/tmp/tb_mv_dst", NULL };
  result("mv exits 0", child_ok(run_exit(mv1)));

  result("mv: src gone", stat("/mnt/tmp/tb_mv_src", &(struct stat){}) != 0);

  read_file("/mnt/tmp/tb_mv_dst", buf, sizeof(buf));
  result("mv: content at dst", strcmp(buf, "mv content\n") == 0);

  unlink("/mnt/tmp/tb_mv_dst");
}

/* -----------------------------------------------------------------------
 * chmod
 * -------------------------------------------------------------------- */
static void test_chmod(void) {
  printf("Test: chmod\n");

  write_file("/mnt/tmp/tb_chmod", "data");

  char *ch1[] = { "/bin/chmod", "755", "/mnt/tmp/tb_chmod", NULL };
  result("chmod exits 0", child_ok(run_exit(ch1)));

  struct stat st;
  stat("/mnt/tmp/tb_chmod", &st);
  result("chmod 755: exec bit set", (st.st_mode & 0111) != 0);

  char *ch2[] = { "/bin/chmod", "644", "/mnt/tmp/tb_chmod", NULL };
  run_exit(ch2);
  stat("/mnt/tmp/tb_chmod", &st);
  result("chmod 644: exec bit cleared", (st.st_mode & 0111) == 0);

  unlink("/mnt/tmp/tb_chmod");
}

/* -----------------------------------------------------------------------
 * ln (hard link) and readlink
 * -------------------------------------------------------------------- */
static void test_ln_readlink(void) {
  printf("Test: ln / readlink\n");
  char buf[256];

  /* Hard link within /tmp */
  write_file("/mnt/tmp/tb_hard_src", "hard link data\n");
  char *ln1[] = { "/bin/ln", "/mnt/tmp/tb_hard_src", "/mnt/tmp/tb_hard_dst", NULL };
  result("ln hard link exits 0", child_ok(run_exit(ln1)));
  read_file("/mnt/tmp/tb_hard_dst", buf, sizeof(buf));
  result("ln hard: content readable via link", strcmp(buf, "hard link data\n") == 0);
  unlink("/mnt/tmp/tb_hard_src");
  unlink("/mnt/tmp/tb_hard_dst");

  /* Symlink within /tmp pointing to a /tmp file */
  write_file("/mnt/tmp/tb_sym_target", "symlink target\n");
  char *ln2[] = { "/bin/ln", "-s", "/mnt/tmp/tb_sym_target", "/mnt/tmp/tb_sym_link", NULL };
  result("ln -s exits 0", child_ok(run_exit(ln2)));

  /* readlink shows the target */
  char *rl1[] = { "/bin/readlink", "/mnt/tmp/tb_sym_link", NULL };
  run_capture(rl1, buf, sizeof(buf));
  /* strip newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink shows target", strcmp(buf, "/mnt/tmp/tb_sym_target") == 0);

  /* stat follows symlink and sees the file */
  struct stat st;
  result("stat follows symlink", stat("/mnt/tmp/tb_sym_link", &st) == 0 && S_ISREG(st.st_mode));

  /* cat through symlink reads target content */
  char *cat1[] = { "/bin/cat", "/mnt/tmp/tb_sym_link", NULL };
  run_capture(cat1, buf, sizeof(buf));
  result("cat through symlink", strcmp(buf, "symlink target\n") == 0);

  unlink("/mnt/tmp/tb_sym_link");
  unlink("/mnt/tmp/tb_sym_target");

  /* Symlink in /tmp pointing across mount into tarfs (/etc/rc) */
  char *ln3[] = { "/bin/ln", "-s", "/etc/rc", "/mnt/tmp/tb_cross_link", NULL };
  result("ln -s cross-mount exits 0", child_ok(run_exit(ln3)));

  char *rl2[] = { "/bin/readlink", "/mnt/tmp/tb_cross_link", NULL };
  run_capture(rl2, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink cross-mount target", strcmp(buf, "/etc/rc") == 0);

  /* stat should follow the symlink into tarfs */
  result("stat cross-mount symlink", stat("/mnt/tmp/tb_cross_link", &st) == 0 && S_ISREG(st.st_mode));

  /* cat through cross-mount symlink */
  char *cat2[] = { "/bin/cat", "/mnt/tmp/tb_cross_link", NULL };
  int s = run_capture(cat2, buf, sizeof(buf));
  result("cat cross-mount symlink exits 0", child_ok(s));
  result("cat cross-mount symlink non-empty", strlen(buf) > 0);

  unlink("/mnt/tmp/tb_cross_link");

  /* Symlink in /tmp pointing to /bin (a directory on tarfs) */
  char *ln4[] = { "/bin/ln", "-s", "/bin", "/mnt/tmp/tb_bin_link", NULL };
  result("ln -s to dir exits 0", child_ok(run_exit(ln4)));
  result("stat symlink-to-dir: is dir", stat("/mnt/tmp/tb_bin_link", &st) == 0 && S_ISDIR(st.st_mode));
  unlink("/mnt/tmp/tb_bin_link");

  /* Relative symlink: link in /tmp pointing to ../etc/rc */
  char *ln5[] = { "/bin/ln", "-s", "../etc/rc", "/mnt/tmp/tb_rel_link", NULL };
  result("ln -s relative exits 0", child_ok(run_exit(ln5)));
  char *rl3[] = { "/bin/readlink", "/mnt/tmp/tb_rel_link", NULL };
  run_capture(rl3, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink relative symlink", strcmp(buf, "../etc/rc") == 0);
  result("stat relative symlink resolves", stat("/mnt/tmp/tb_rel_link", &st) == 0 && S_ISREG(st.st_mode));
  unlink("/mnt/tmp/tb_rel_link");
}

/* -----------------------------------------------------------------------
 * tail
 * -------------------------------------------------------------------- */
static void test_tail(void) {
  printf("Test: tail\n");
  char buf[256];

  /* Write a 5-line file */
  write_file("/mnt/tmp/tb_tail", "line1\nline2\nline3\nline4\nline5\n");

  char *t1[] = { "/bin/tail", "-n", "2", "/mnt/tmp/tb_tail", NULL };
  run_capture(t1, buf, sizeof(buf));
  result("tail -n 2: last 2 lines", strcmp(buf, "line4\nline5\n") == 0);

  char *t2[] = { "/bin/tail", "-n", "1", "/mnt/tmp/tb_tail", NULL };
  run_capture(t2, buf, sizeof(buf));
  result("tail -n 1: last line", strcmp(buf, "line5\n") == 0);

  unlink("/mnt/tmp/tb_tail");
}

/* -----------------------------------------------------------------------
 * ls
 * -------------------------------------------------------------------- */
static void test_ls(void) {
  printf("Test: ls\n");
  char buf[1024];

  /* ls /bin lists binaries */
  char *a1[] = { "/bin/ls", "/bin", NULL };
  int s = run_capture(a1, buf, sizeof(buf));
  result("ls /bin exits 0", child_ok(s));
  result("ls /bin contains 'echo'", strstr(buf, "echo") != NULL);
  result("ls /bin contains 'sh'",   strstr(buf, "sh")   != NULL);

  /* ls /tmp should work (writable fs) */
  char *a2[] = { "/bin/ls", "/mnt/tmp", NULL };
  s = run_capture(a2, buf, sizeof(buf));
  result("ls /tmp exits 0", child_ok(s));

  /* ls -l shows permission bits */
  char *a3[] = { "/bin/ls", "-l", "/bin", NULL };
  s = run_capture(a3, buf, sizeof(buf));
  result("ls -l exits 0", child_ok(s));
  result("ls -l output has mode chars", strstr(buf, "rwx") != NULL || strstr(buf, "r-x") != NULL);

  /* ls nonexistent exits non-zero */
  char *a4[] = { "/bin/ls", "/no/such/dir", NULL };
  s = run_exit(a4);
  result("ls missing dir exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* ls a file we created */
  write_file("/mnt/tmp/tb_ls_file", "x");
  char *a5[] = { "/bin/ls", "/mnt/tmp", NULL };
  run_capture(a5, buf, sizeof(buf));
  result("ls shows newly created file", strstr(buf, "tb_ls_file") != NULL);
  unlink("/mnt/tmp/tb_ls_file");
}

/* -----------------------------------------------------------------------
 * cat — additional coverage
 * -------------------------------------------------------------------- */
static void test_cat(void) {
  printf("Test: cat\n");
  char buf[256];

  /* cat two files concatenates them */
  write_file("/mnt/tmp/tb_cat_a", "aaa\n");
  write_file("/mnt/tmp/tb_cat_b", "bbb\n");
  char *a1[] = { "/bin/cat", "/mnt/tmp/tb_cat_a", "/mnt/tmp/tb_cat_b", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("cat two files", strcmp(buf, "aaa\nbbb\n") == 0);

  /* cat missing file exits non-zero */
  char *a2[] = { "/bin/cat", "/no/such/file", NULL };
  int s = run_exit(a2);
  result("cat missing file exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* cat stdin via pipe */
  int in_fds[2], out_fds[2];
  pipe(in_fds); pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0], 0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *ca[] = { "/bin/cat", NULL };
    execve("/bin/cat", ca, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  write(in_fds[1], "stdin data\n", 11);
  close(in_fds[1]);
  size_t tot = 0; ssize_t n;
  while (tot < sizeof(buf)-1 && (n = read(out_fds[0], buf+tot, sizeof(buf)-1-tot)) > 0)
    tot += n;
  buf[tot] = '\0';
  close(out_fds[0]);
  int st; waitpid(pid, &st, 0);
  result("cat stdin passthrough", strcmp(buf, "stdin data\n") == 0);

  unlink("/mnt/tmp/tb_cat_a");
  unlink("/mnt/tmp/tb_cat_b");
}

/* -----------------------------------------------------------------------
 * touch — additional coverage
 * -------------------------------------------------------------------- */
static void test_touch(void) {
  printf("Test: touch\n");

  /* touch creates a file */
  char *a1[] = { "/bin/touch", "/mnt/tmp/tb_touch2", NULL };
  result("touch creates file", child_ok(run_exit(a1)));
  struct stat st;
  result("touch: file is regular", stat("/mnt/tmp/tb_touch2", &st) == 0 && S_ISREG(st.st_mode));

  /* touch existing file is idempotent (exits 0) */
  char *a2[] = { "/bin/touch", "/mnt/tmp/tb_touch2", NULL };
  result("touch existing is idempotent", child_ok(run_exit(a2)));

  /* touch multiple files */
  char *a3[] = { "/bin/touch", "/mnt/tmp/tb_touch3", "/mnt/tmp/tb_touch4", NULL };
  result("touch multiple files", child_ok(run_exit(a3)));
  result("touch multi: first exists",  stat("/mnt/tmp/tb_touch3", &st) == 0);
  result("touch multi: second exists", stat("/mnt/tmp/tb_touch4", &st) == 0);

  unlink("/mnt/tmp/tb_touch2");
  unlink("/mnt/tmp/tb_touch3");
  unlink("/mnt/tmp/tb_touch4");
}

/* -----------------------------------------------------------------------
 * cp — additional coverage
 * -------------------------------------------------------------------- */
static void test_cp_extra(void) {
  printf("Test: cp (extra)\n");
  char buf[256];

  /* cp overwrites existing file */
  write_file("/mnt/tmp/tb_cp_src", "version1\n");
  write_file("/mnt/tmp/tb_cp_dst", "old content\n");
  char *a1[] = { "/bin/cp", "/mnt/tmp/tb_cp_src", "/mnt/tmp/tb_cp_dst", NULL };
  result("cp overwrite exits 0", child_ok(run_exit(a1)));
  read_file("/mnt/tmp/tb_cp_dst", buf, sizeof(buf));
  result("cp overwrite: dst has new content", strcmp(buf, "version1\n") == 0);

  /* cp from tarfs (read-only) to /tmp (writable) */
  char *a2[] = { "/bin/cp", "/etc/rc", "/mnt/tmp/tb_cp_rc", NULL };
  result("cp tarfs->tmp exits 0", child_ok(run_exit(a2)));
  result("cp tarfs->tmp: dst exists", stat("/mnt/tmp/tb_cp_rc", &(struct stat){}) == 0);

  /* cp missing src exits non-zero */
  char *a3[] = { "/bin/cp", "/no/such", "/mnt/tmp/tb_cp_dst", NULL };
  int s = run_exit(a3);
  result("cp missing src exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  unlink("/mnt/tmp/tb_cp_src");
  unlink("/mnt/tmp/tb_cp_dst");
  unlink("/mnt/tmp/tb_cp_rc");
}

/* -----------------------------------------------------------------------
 * rm — additional coverage
 * -------------------------------------------------------------------- */
static void test_rm_extra(void) {
  printf("Test: rm (extra)\n");

  /* rm missing file exits non-zero */
  char *a1[] = { "/bin/rm", "/mnt/tmp/no_such_file_xyz", NULL };
  int s = run_exit(a1);
  result("rm missing file exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* rm multiple files */
  write_file("/mnt/tmp/tb_rm1", "a");
  write_file("/mnt/tmp/tb_rm2", "b");
  char *a2[] = { "/bin/rm", "/mnt/tmp/tb_rm1", "/mnt/tmp/tb_rm2", NULL };
  result("rm multiple files exits 0", child_ok(run_exit(a2)));
  result("rm multi: first gone",  stat("/mnt/tmp/tb_rm1", &(struct stat){}) != 0);
  result("rm multi: second gone", stat("/mnt/tmp/tb_rm2", &(struct stat){}) != 0);
}

/* -----------------------------------------------------------------------
 * tail — additional coverage
 * -------------------------------------------------------------------- */
static void test_tail_extra(void) {
  printf("Test: tail (extra)\n");
  char buf[256];

  /* requesting more lines than exist returns the whole file */
  write_file("/mnt/tmp/tb_tail2", "only\ntwo\n");
  char *a1[] = { "/bin/tail", "-n", "10", "/mnt/tmp/tb_tail2", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("tail -n 10 on 2-line file returns all", strcmp(buf, "only\ntwo\n") == 0);

  /* exact line count */
  char *a2[] = { "/bin/tail", "-n", "2", "/mnt/tmp/tb_tail2", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("tail -n 2 exact match", strcmp(buf, "only\ntwo\n") == 0);

  /* tail -c bytes */
  char *a3[] = { "/bin/tail", "-c", "4", "/mnt/tmp/tb_tail2", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("tail -c 4: last 4 bytes", strcmp(buf, "two\n") == 0);

  unlink("/mnt/tmp/tb_tail2");
}

/* -----------------------------------------------------------------------
 * df
 * -------------------------------------------------------------------- */
static void test_df(void) {
  printf("Test: df\n");
  char buf[512];

  char *a1[] = { "/bin/df", NULL };
  int s = run_capture(a1, buf, sizeof(buf));
  result("df exits 0", child_ok(s));
  result("df output has 'Filesystem' header", strstr(buf, "Filesystem") != NULL);
  result("df output has 'Use%'", strstr(buf, "Use%") != NULL);

  char *a2[] = { "/bin/df", "/mnt/tmp", NULL };
  s = run_capture(a2, buf, sizeof(buf));
  result("df /tmp exits 0", child_ok(s));
}

/* -----------------------------------------------------------------------
 * echo — additional coverage (redirected output via shell not testable here,
 *        but we can test more arg combinations)
 * -------------------------------------------------------------------- */
static void test_echo_extra(void) {
  printf("Test: echo (extra)\n");
  char buf[128];

  /* echo with special chars */
  char *a1[] = { "/bin/echo", "hello", "world", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("echo two args space-separated", strcmp(buf, "hello world\n") == 0);

  /* echo -n with multiple args */
  char *a2[] = { "/bin/echo", "-n", "a", "b", "c", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("echo -n multiple args", strcmp(buf, "a b c") == 0);

  /* echo empty string arg */
  char *a3[] = { "/bin/echo", "", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("echo empty string arg prints newline", strcmp(buf, "\n") == 0);
}

/* -----------------------------------------------------------------------
 * test binary — additional coverage
 * -------------------------------------------------------------------- */
static void test_test_extra(void) {
  printf("Test: test (extra)\n");

  /* -e tests existence regardless of type */
  char *t1[] = { "/bin/test", "-e", "/bin", NULL };
  result("test -e directory", child_ok(run_exit(t1)));

  char *t2[] = { "/bin/test", "-e", "/etc/rc", NULL };
  result("test -e file", child_ok(run_exit(t2)));

  char *t3[] = { "/bin/test", "-e", "/no/such", NULL };
  int s = run_exit(t3);
  result("test -e missing exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  /* -s: file with content */
  write_file("/mnt/tmp/tb_test_s", "data");
  char *t4[] = { "/bin/test", "-s", "/mnt/tmp/tb_test_s", NULL };
  result("test -s non-empty file", child_ok(run_exit(t4)));
  unlink("/mnt/tmp/tb_test_s");

  /* -z and -n */
  char *t5[] = { "/bin/test", "-z", "", NULL };
  result("test -z empty", child_ok(run_exit(t5)));
  char *t6[] = { "/bin/test", "-z", "x", NULL };
  s = run_exit(t6);
  result("test -z non-empty exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  /* negation */
  char *t7[] = { "/bin/test", "!", "-f", "/no/such", NULL };
  result("test ! -f missing", child_ok(run_exit(t7)));
  char *t8[] = { "/bin/test", "!", "-f", "/etc/rc", NULL };
  s = run_exit(t8);
  result("test ! -f existing exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  /* integer comparisons */
  char *t9[]  = { "/bin/test", "5", "-le", "5", NULL };
  result("test 5 -le 5", child_ok(run_exit(t9)));
  char *t10[] = { "/bin/test", "4", "-lt", "5", NULL };
  result("test 4 -lt 5", child_ok(run_exit(t10)));
  char *t11[] = { "/bin/test", "5", "-ge", "5", NULL };
  result("test 5 -ge 5", child_ok(run_exit(t11)));
  char *t12[] = { "/bin/test", "0", "-eq", "0", NULL };
  result("test 0 -eq 0", child_ok(run_exit(t12)));
  char *t13[] = { "/bin/test", "1", "-ne", "2", NULL };
  result("test 1 -ne 2", child_ok(run_exit(t13)));
}

/* -----------------------------------------------------------------------
 * new options: ls -a, wc -l/-w/-c, rm -r, sleep fractional
 * -------------------------------------------------------------------- */
static void test_new_options(void) {
  printf("Test: new options\n");
  char buf[512];

  /* ls -a shows dot files */
  write_file("/mnt/tmp/.tb_hidden", "x");
  char *a1[] = { "/bin/ls", "-a", "/mnt/tmp", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("ls -a shows dotfiles", strstr(buf, ".tb_hidden") != NULL);
  /* ls without -a hides dot files */
  char *a2[] = { "/bin/ls", "/mnt/tmp", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("ls without -a hides dotfiles", strstr(buf, ".tb_hidden") == NULL);
  unlink("/mnt/tmp/.tb_hidden");

  /* wc -l counts only lines */
  write_file("/mnt/tmp/tb_wc_opt", "a\nb\nc\n");
  char *w1[] = { "/bin/wc", "-l", "/mnt/tmp/tb_wc_opt", NULL };
  run_capture(w1, buf, sizeof(buf));
  int lines = 0, words = 0, bytes = 0;
  sscanf(buf, "%d", &lines);
  result("wc -l: only line count", lines == 3);

  /* wc -w counts only words */
  char *w2[] = { "/bin/wc", "-w", "/mnt/tmp/tb_wc_opt", NULL };
  run_capture(w2, buf, sizeof(buf));
  sscanf(buf, "%d", &words);
  result("wc -w: only word count", words == 3);

  /* wc -c counts only bytes */
  char *w3[] = { "/bin/wc", "-c", "/mnt/tmp/tb_wc_opt", NULL };
  run_capture(w3, buf, sizeof(buf));
  sscanf(buf, "%d", &bytes);
  result("wc -c: only byte count", bytes == 6);

  /* wc -lc shows two columns */
  char *w4[] = { "/bin/wc", "-lc", "/mnt/tmp/tb_wc_opt", NULL };
  run_capture(w4, buf, sizeof(buf));
  int a = 0, b2 = 0;
  int matched = sscanf(buf, "%d %d", &a, &b2);
  result("wc -lc: two columns", matched == 2 && a == 3 && b2 == 6);
  unlink("/mnt/tmp/tb_wc_opt");

  /* rm -r removes directory tree */
  char *m1[] = { "/bin/mkdir", "-p", "/mnt/tmp/tb_rmr/sub", NULL };
  run_exit(m1);
  write_file("/mnt/tmp/tb_rmr/f1", "x");
  write_file("/mnt/tmp/tb_rmr/sub/f2", "y");
  char *r1[] = { "/bin/rm", "-r", "/mnt/tmp/tb_rmr", NULL };
  result("rm -r exits 0", child_ok(run_exit(r1)));
  result("rm -r: tree gone", stat("/mnt/tmp/tb_rmr", &(struct stat){}) != 0);

  /* sleep basic sanity */
  char *s1[] = { "/bin/sleep", "0", NULL };
  result("sleep 0 exits 0", child_ok(run_exit(s1)));
}

/* -----------------------------------------------------------------------
 * head
 * -------------------------------------------------------------------- */
static void test_head(void) {
  printf("Test: head\n");
  char buf[256];

  write_file("/mnt/tmp/tb_head", "line1\nline2\nline3\nline4\nline5\n");

  /* default 10 lines returns whole 5-line file */
  char *a1[] = { "/bin/head", "/mnt/tmp/tb_head", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("head default: returns all lines when fewer than 10",
         strcmp(buf, "line1\nline2\nline3\nline4\nline5\n") == 0);

  /* -n 3 returns first 3 */
  char *a2[] = { "/bin/head", "-n", "3", "/mnt/tmp/tb_head", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("head -n 3: first 3 lines", strcmp(buf, "line1\nline2\nline3\n") == 0);

  /* -n 1 returns first line */
  char *a3[] = { "/bin/head", "-n", "1", "/mnt/tmp/tb_head", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("head -n 1: first line only", strcmp(buf, "line1\n") == 0);

  /* -n more than file returns whole file */
  char *a4[] = { "/bin/head", "-n", "20", "/mnt/tmp/tb_head", NULL };
  run_capture(a4, buf, sizeof(buf));
  result("head -n 20 on 5-line file: returns all",
         strcmp(buf, "line1\nline2\nline3\nline4\nline5\n") == 0);

  /* shorthand -2 */
  char *a5[] = { "/bin/head", "-2", "/mnt/tmp/tb_head", NULL };
  run_capture(a5, buf, sizeof(buf));
  result("head -2 shorthand: first 2 lines", strcmp(buf, "line1\nline2\n") == 0);

  /* stdin */
  int in_fds[2], out_fds[2];
  pipe(in_fds); pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0], 0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *ha[] = { "/bin/head", "-n", "2", NULL };
    execve("/bin/head", ha, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  write(in_fds[1], "a\nb\nc\nd\n", 8);
  close(in_fds[1]);
  size_t tot = 0; ssize_t n;
  while (tot < sizeof(buf)-1 && (n = read(out_fds[0], buf+tot, sizeof(buf)-1-tot)) > 0)
    tot += n;
  buf[tot] = '\0';
  close(out_fds[0]);
  int st; waitpid(pid, &st, 0);
  result("head stdin -n 2", strcmp(buf, "a\nb\n") == 0);

  /* missing file exits non-zero */
  char *a6[] = { "/bin/head", "/no/such/file", NULL };
  int s = run_exit(a6);
  result("head missing file exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* multiple files shows headers */
  write_file("/mnt/tmp/tb_head2", "x\ny\n");
  char *a7[] = { "/bin/head", "-n", "1", "/mnt/tmp/tb_head", "/mnt/tmp/tb_head2", NULL };
  run_capture(a7, buf, sizeof(buf));
  result("head multiple files shows ==> headers", strstr(buf, "==>") != NULL);
  result("head multiple files: first file content", strstr(buf, "line1") != NULL);
  result("head multiple files: second file content", strstr(buf, "x") != NULL);
  unlink("/mnt/tmp/tb_head2");

  unlink("/mnt/tmp/tb_head");
}

/* -----------------------------------------------------------------------
 * date
 * -------------------------------------------------------------------- */
static void test_date(void) {
  printf("Test: date\n");
  char buf[128];

  /* date exits 0 and produces output */
  char *a1[] = { "/bin/date", NULL };
  int s = run_capture(a1, buf, sizeof(buf));
  result("date exits 0", child_ok(s));
  result("date output non-empty", strlen(buf) > 0);

  /* Output contains UTC */
  result("date output contains 'UTC'", strstr(buf, "UTC") != NULL);

  /* Format string: +%Y gives a 4-digit year */
  char *a2[] = { "/bin/date", "+%Y", NULL };
  run_capture(a2, buf, sizeof(buf));
  /* strip newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("date +%Y: 4-digit year", strlen(buf) == 4);
  int year = atoi(buf);
  result("date +%Y: year >= 2024", year >= 2024);

  /* Format string: +%H:%M:%S produces HH:MM:SS */
  char *a3[] = { "/bin/date", "+%H:%M:%S", NULL };
  run_capture(a3, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("date +%H:%M:%S: 8 chars", strlen(buf) == 8);
  result("date +%H:%M:%S: contains colons", buf[2] == ':' && buf[5] == ':');

  /* +%Y-%m-%d produces YYYY-MM-DD */
  char *a4[] = { "/bin/date", "+%Y-%m-%d", NULL };
  run_capture(a4, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("date +%Y-%m-%d: 10 chars", strlen(buf) == 10);
  result("date +%Y-%m-%d: has dashes", buf[4] == '-' && buf[7] == '-');
}

/* -----------------------------------------------------------------------
 * printf binary
 * -------------------------------------------------------------------- */
static void test_printf(void) {
  printf("Test: printf\n");
  char buf[256];

  /* Basic string */
  char *a1[] = { "/bin/printf", "%s", "hello", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("printf %s", strcmp(buf, "hello") == 0);

  /* String with newline escape in format */
  char *a2[] = { "/bin/printf", "%s\n", "world", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("printf %s\\n", strcmp(buf, "world\n") == 0);

  /* Decimal integer */
  char *a3[] = { "/bin/printf", "%d\n", "42", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("printf %d", strcmp(buf, "42\n") == 0);

  /* Negative integer */
  char *a4[] = { "/bin/printf", "%d\n", "-7", NULL };
  run_capture(a4, buf, sizeof(buf));
  result("printf %d negative", strcmp(buf, "-7\n") == 0);

  /* Octal */
  char *a5[] = { "/bin/printf", "%o\n", "8", NULL };
  run_capture(a5, buf, sizeof(buf));
  result("printf %o", strcmp(buf, "10\n") == 0);

  /* Hex lower */
  char *a6[] = { "/bin/printf", "%x\n", "255", NULL };
  run_capture(a6, buf, sizeof(buf));
  result("printf %x", strcmp(buf, "ff\n") == 0);

  /* Hex upper */
  char *a7[] = { "/bin/printf", "%X\n", "255", NULL };
  run_capture(a7, buf, sizeof(buf));
  result("printf %X", strcmp(buf, "FF\n") == 0);

  /* Unsigned */
  char *a8[] = { "/bin/printf", "%u\n", "4294967295", NULL };
  run_capture(a8, buf, sizeof(buf));
  result("printf %u large", strcmp(buf, "4294967295\n") == 0);

  /* Character */
  char *a9[] = { "/bin/printf", "%c\n", "A", NULL };
  run_capture(a9, buf, sizeof(buf));
  result("printf %c", strcmp(buf, "A\n") == 0);

  /* Width right-align */
  char *a10[] = { "/bin/printf", "%5d\n", "42", NULL };
  run_capture(a10, buf, sizeof(buf));
  result("printf %5d right-align", strcmp(buf, "   42\n") == 0);

  /* Width left-align */
  char *a11[] = { "/bin/printf", "%-5d|\n", "42", NULL };
  run_capture(a11, buf, sizeof(buf));
  result("printf %-5d left-align", strcmp(buf, "42   |\n") == 0);

  /* Zero-pad */
  char *a12[] = { "/bin/printf", "%05d\n", "42", NULL };
  run_capture(a12, buf, sizeof(buf));
  result("printf %05d zero-pad", strcmp(buf, "00042\n") == 0);

  /* String width */
  char *a13[] = { "/bin/printf", "%10s\n", "hi", NULL };
  run_capture(a13, buf, sizeof(buf));
  result("printf %10s right-align", strcmp(buf, "        hi\n") == 0);

  /* String left-align */
  char *a14[] = { "/bin/printf", "%-10s|\n", "hi", NULL };
  run_capture(a14, buf, sizeof(buf));
  result("printf %-10s left-align", strcmp(buf, "hi        |\n") == 0);

  /* Escape sequences in format: \n \t \\ */
  char *a15[] = { "/bin/printf", "a\tb\nc\n", NULL };
  run_capture(a15, buf, sizeof(buf));
  result("printf \\t \\n escapes", strcmp(buf, "a\tb\nc\n") == 0);

  /* \a \b \r escapes */
  char *a16[] = { "/bin/printf", "\a\b\r", NULL };
  run_capture(a16, buf, sizeof(buf));
  result("printf \\a \\b \\r escapes", buf[0] == '\a' && buf[1] == '\b' && buf[2] == '\r');

  /* Octal escape \0NNN */
  char *a17[] = { "/bin/printf", "\101\n", NULL };
  run_capture(a17, buf, sizeof(buf));
  result("printf octal \\101 = A", strcmp(buf, "A\n") == 0);

  /* Hex escape \xHH */
  char *a18[] = { "/bin/printf", "\x41\n", NULL };
  run_capture(a18, buf, sizeof(buf));
  result("printf hex \\x41 = A", strcmp(buf, "A\n") == 0);

  /* %% literal percent */
  char *a19[] = { "/bin/printf", "100%%\n", NULL };
  run_capture(a19, buf, sizeof(buf));
  result("printf %% literal", strcmp(buf, "100%\n") == 0);

  /* Multiple args, format reused */
  char *a20[] = { "/bin/printf", "%d\n", "1", "2", "3", NULL };
  run_capture(a20, buf, sizeof(buf));
  result("printf format reuse", strcmp(buf, "1\n2\n3\n") == 0);

  /* Multiple format specifiers in one format */
  char *a21[] = { "/bin/printf", "%s=%d\n", "x", "5", NULL };
  run_capture(a21, buf, sizeof(buf));
  result("printf multiple specifiers", strcmp(buf, "x=5\n") == 0);

  /* Hex with 0x prefix using %#x — libc may not support #, just check no crash */
  char *a22[] = { "/bin/printf", "%i\n", "010", NULL };
  run_capture(a22, buf, sizeof(buf));
  result("printf %i octal input 010=8", strcmp(buf, "8\n") == 0);

  /* %i hex input */
  char *a23[] = { "/bin/printf", "%i\n", "0x1f", NULL };
  run_capture(a23, buf, sizeof(buf));
  result("printf %i hex input 0x1f=31", strcmp(buf, "31\n") == 0);

  /* \\ double backslash */
  char *a24[] = { "/bin/printf", "a\\\\b\n", NULL };
  run_capture(a24, buf, sizeof(buf));
  result("printf \\\\\\\\ = single backslash", strcmp(buf, "a\\b\n") == 0);

  /* Empty format */
  char *a25[] = { "/bin/printf", "", NULL };
  run_capture(a25, buf, sizeof(buf));
  result("printf empty format produces no output", strcmp(buf, "") == 0);

  /* No args past format, missing arg treated as empty/0 */
  char *a26[] = { "/bin/printf", "%s\n", NULL };
  run_capture(a26, buf, sizeof(buf));
  result("printf missing string arg is empty", strcmp(buf, "\n") == 0);

  char *a27[] = { "/bin/printf", "%d\n", NULL };
  run_capture(a27, buf, sizeof(buf));
  result("printf missing int arg is 0", strcmp(buf, "0\n") == 0);

  /* exit code 0 on success */
  char *a28[] = { "/bin/printf", "hi", NULL };
  int st = run_exit(a28);
  result("printf exits 0", WIFEXITED(st) && WEXITSTATUS(st) == 0);

  /* exit code nonzero with no format */
  char *a29[] = { "/bin/printf", NULL };
  st = run_exit(a29);
  result("printf no args exits nonzero", WIFEXITED(st) && WEXITSTATUS(st) != 0);

  /* %b interprets escapes in argument */
  char *a30[] = { "/bin/printf", "%b\n", "hello\\nworld", NULL };
  run_capture(a30, buf, sizeof(buf));
  result("printf %b interprets \\n in arg", strcmp(buf, "hello\nworld\n") == 0);

  /* \c stops output */
  char *a31[] = { "/bin/printf", "hello\\cworld", NULL };
  run_capture(a31, buf, sizeof(buf));
  result("printf \\c stops output", strcmp(buf, "hello") == 0);
}

/* -----------------------------------------------------------------------
 * sleep
 * -------------------------------------------------------------------- */
static void test_sleep(void) {
  printf("Test: sleep\n");

  /* sleep 0 exits 0 immediately */
  char *a1[] = { "/bin/sleep", "0", NULL };
  result("sleep 0 exits 0", child_ok(run_exit(a1)));

  /* sleep 1 exits 0 */
  char *a2[] = { "/bin/sleep", "1", NULL };
  result("sleep 1 exits 0", child_ok(run_exit(a2)));

  /* sleep with no args exits non-zero */
  char *a3[] = { "/bin/sleep", NULL };
  int s = run_exit(a3);
  result("sleep no args exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);
}

/* -----------------------------------------------------------------------
 * dmesg
 * -------------------------------------------------------------------- */
static void test_dmesg(void) {
  printf("Test: dmesg\n");
  char buf[4096];

  char *a1[] = { "/bin/dmesg", NULL };
  int s = run_capture(a1, buf, sizeof(buf));
  result("dmesg exits 0", child_ok(s));
  result("dmesg output non-empty", strlen(buf) > 0);
}

/* -----------------------------------------------------------------------
 * env
 * -------------------------------------------------------------------- */
static void test_env(void) {
  printf("Test: env\n");
  char buf[1024];

  /* env with no args prints environment — should contain PATH */
  char *a1[] = { "/bin/env", NULL };
  int s = run_capture(a1, buf, sizeof(buf));
  result("env exits 0", child_ok(s));
  result("env output contains PATH=", strstr(buf, "PATH=") != NULL);

  /* env NAME=val command: command sees the variable */
  char *a2[] = { "/bin/env", "TESTVAR=hello", "/bin/env", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("env NAME=val passes var to command", strstr(buf, "TESTVAR=hello") != NULL);

  /* env -i clears environment and runs command cleanly */
  char *a3[] = { "/bin/env", "-i", "ONLY=x", "/bin/env", NULL };
  run_capture(a3, buf, sizeof(buf));
  result("env -i: only set vars present", strstr(buf, "ONLY=x") != NULL);
  result("env -i: PATH not present", strstr(buf, "PATH=") == NULL);

  /* env with unknown command exits 127 (name-only so env does PATH lookup) */
  char *a4[] = { "/bin/env", "no_such_cmd_xyz", NULL };
  s = run_exit(a4);
  result("env unknown command exits 127", WIFEXITED(s) && WEXITSTATUS(s) == 127);
}

/* -----------------------------------------------------------------------
 * rm -f and -rf
 * -------------------------------------------------------------------- */
static void test_rm_force(void) {
  printf("Test: rm -f / -rf\n");

  /* rm -f on nonexistent file exits 0 */
  char *a1[] = { "/bin/rm", "-f", "/mnt/tmp/no_such_file_rmf_xyz", NULL };
  result("rm -f nonexistent exits 0", child_ok(run_exit(a1)));

  /* rm -rf on nonexistent dir exits 0 */
  char *a2[] = { "/bin/rm", "-rf", "/mnt/tmp/no_such_dir_rmrf_xyz", NULL };
  result("rm -rf nonexistent exits 0", child_ok(run_exit(a2)));

  /* rm -rf removes nested tree */
  char *m1[] = { "/bin/mkdir", "-p", "/mnt/tmp/tb_rmrf/a/b", NULL };
  run_exit(m1);
  write_file("/mnt/tmp/tb_rmrf/a/f1", "x");
  write_file("/mnt/tmp/tb_rmrf/a/b/f2", "y");
  char *a3[] = { "/bin/rm", "-rf", "/mnt/tmp/tb_rmrf", NULL };
  result("rm -rf nested tree exits 0", child_ok(run_exit(a3)));
  result("rm -rf: tree gone", stat("/mnt/tmp/tb_rmrf", &(struct stat){}) != 0);

  /* rm -f on existing file removes it */
  write_file("/mnt/tmp/tb_rmf_file", "data");
  char *a4[] = { "/bin/rm", "-f", "/mnt/tmp/tb_rmf_file", NULL };
  result("rm -f existing file exits 0", child_ok(run_exit(a4)));
  result("rm -f: file gone", stat("/mnt/tmp/tb_rmf_file", &(struct stat){}) != 0);

  /* rm -rf on empty dir */
  char *m2[] = { "/bin/mkdir", "/mnt/tmp/tb_rmrf_empty", NULL };
  run_exit(m2);
  char *a5[] = { "/bin/rm", "-rf", "/mnt/tmp/tb_rmrf_empty", NULL };
  result("rm -rf empty dir exits 0", child_ok(run_exit(a5)));
  result("rm -rf: empty dir gone", stat("/mnt/tmp/tb_rmrf_empty", &(struct stat){}) != 0);
}

/* -----------------------------------------------------------------------
 * cp -r (recursive copy)
 * -------------------------------------------------------------------- */
static void test_cp_recursive(void) {
  printf("Test: cp -r\n");
  char buf[256];

  /* cp -r a directory tree */
  char *m1[] = { "/bin/mkdir", "-p", "/mnt/tmp/tb_cpr_src/sub", NULL };
  run_exit(m1);
  write_file("/mnt/tmp/tb_cpr_src/f1", "file1\n");
  write_file("/mnt/tmp/tb_cpr_src/sub/f2", "file2\n");

  char *a1[] = { "/bin/cp", "-r", "/mnt/tmp/tb_cpr_src", "/mnt/tmp/tb_cpr_dst", NULL };
  result("cp -r exits 0", child_ok(run_exit(a1)));

  struct stat st;
  result("cp -r: dst dir exists", stat("/mnt/tmp/tb_cpr_dst", &st) == 0 && S_ISDIR(st.st_mode));
  result("cp -r: file copied", stat("/mnt/tmp/tb_cpr_dst/f1", &st) == 0);
  read_file("/mnt/tmp/tb_cpr_dst/f1", buf, sizeof(buf));
  result("cp -r: file content preserved", strcmp(buf, "file1\n") == 0);
  result("cp -r: subdir exists", stat("/mnt/tmp/tb_cpr_dst/sub", &st) == 0 && S_ISDIR(st.st_mode));
  result("cp -r: nested file copied", stat("/mnt/tmp/tb_cpr_dst/sub/f2", &st) == 0);
  read_file("/mnt/tmp/tb_cpr_dst/sub/f2", buf, sizeof(buf));
  result("cp -r: nested file content", strcmp(buf, "file2\n") == 0);

  /* cleanup */
  char *r1[] = { "/bin/rm", "-rf", "/mnt/tmp/tb_cpr_src", NULL };
  char *r2[] = { "/bin/rm", "-rf", "/mnt/tmp/tb_cpr_dst", NULL };
  run_exit(r1); run_exit(r2);
}

/* -----------------------------------------------------------------------
 * mv — directory rename
 * -------------------------------------------------------------------- */
static void test_mv_dir(void) {
  printf("Test: mv dir\n");
  char buf[256];

  /* mv a directory */
  char *m1[] = { "/bin/mkdir", "/mnt/tmp/tb_mvd_src", NULL };
  run_exit(m1);
  write_file("/mnt/tmp/tb_mvd_src/file", "content\n");

  char *a1[] = { "/bin/mv", "/mnt/tmp/tb_mvd_src", "/mnt/tmp/tb_mvd_dst", NULL };
  result("mv dir exits 0", child_ok(run_exit(a1)));

  struct stat st;
  result("mv dir: src gone", stat("/mnt/tmp/tb_mvd_src", &st) != 0);
  result("mv dir: dst exists", stat("/mnt/tmp/tb_mvd_dst", &st) == 0 && S_ISDIR(st.st_mode));
  read_file("/mnt/tmp/tb_mvd_dst/file", buf, sizeof(buf));
  result("mv dir: file content preserved", strcmp(buf, "content\n") == 0);

  char *r1[] = { "/bin/rm", "-rf", "/mnt/tmp/tb_mvd_dst", NULL };
  run_exit(r1);
}

/* -----------------------------------------------------------------------
 * wc — edge cases
 * -------------------------------------------------------------------- */
static void test_wc_edge(void) {
  printf("Test: wc edge cases\n");
  char buf[128];

  /* empty file */
  write_file("/mnt/tmp/tb_wc_empty", "");
  char *a1[] = { "/bin/wc", "/mnt/tmp/tb_wc_empty", NULL };
  run_capture(a1, buf, sizeof(buf));
  long l, w, b;
  int m = sscanf(buf, "%ld %ld %ld", &l, &w, &b);
  result("wc empty file: 0 lines 0 words 0 bytes", m == 3 && l == 0 && w == 0 && b == 0);
  unlink("/mnt/tmp/tb_wc_empty");

  /* single word no trailing newline */
  write_file("/mnt/tmp/tb_wc_noln", "word");
  char *a2[] = { "/bin/wc", "/mnt/tmp/tb_wc_noln", NULL };
  run_capture(a2, buf, sizeof(buf));
  m = sscanf(buf, "%ld %ld %ld", &l, &w, &b);
  result("wc no-newline: 0 lines 1 word 4 bytes", m == 3 && l == 0 && w == 1 && b == 4);
  unlink("/mnt/tmp/tb_wc_noln");
}

/* -----------------------------------------------------------------------
 * chmod — more modes
 * -------------------------------------------------------------------- */
static void test_chmod_extra(void) {
  printf("Test: chmod (extra)\n");

  write_file("/mnt/tmp/tb_chmod2", "data");

  /* chmod 000 clears all bits */
  char *a1[] = { "/bin/chmod", "000", "/mnt/tmp/tb_chmod2", NULL };
  result("chmod 000 exits 0", child_ok(run_exit(a1)));
  struct stat st;
  stat("/mnt/tmp/tb_chmod2", &st);
  result("chmod 000: no permission bits", (st.st_mode & 0777) == 0);

  /* chmod 777 sets all bits */
  char *a2[] = { "/bin/chmod", "777", "/mnt/tmp/tb_chmod2", NULL };
  result("chmod 777 exits 0", child_ok(run_exit(a2)));
  stat("/mnt/tmp/tb_chmod2", &st);
  result("chmod 777: all bits set", (st.st_mode & 0777) == 0777);

  /* chmod 400 read-only owner */
  char *a3[] = { "/bin/chmod", "400", "/mnt/tmp/tb_chmod2", NULL };
  result("chmod 400 exits 0", child_ok(run_exit(a3)));
  stat("/mnt/tmp/tb_chmod2", &st);
  result("chmod 400: owner read only", (st.st_mode & 0777) == 0400);

  /* chmod on nonexistent exits non-zero */
  char *a4[] = { "/bin/chmod", "755", "/mnt/tmp/no_such_chmod_xyz", NULL };
  int s = run_exit(a4);
  result("chmod nonexistent exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);

  /* chmod 644 to restore for unlink */
  char *a5[] = { "/bin/chmod", "644", "/mnt/tmp/tb_chmod2", NULL };
  run_exit(a5);
  unlink("/mnt/tmp/tb_chmod2");
}

/* -----------------------------------------------------------------------
 * cat — binary/large data passthrough
 * -------------------------------------------------------------------- */
static void test_cat_extra(void) {
  printf("Test: cat (extra)\n");
  char buf[512];

  /* cat with - reads stdin */
  int in_fds[2], out_fds[2];
  pipe(in_fds); pipe(out_fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(in_fds[1]); close(out_fds[0]);
    dup2(in_fds[0], 0); close(in_fds[0]);
    dup2(out_fds[1], 1); close(out_fds[1]);
    char *ca[] = { "/bin/cat", "-", NULL };
    execve("/bin/cat", ca, NULL);
    _exit(127);
  }
  close(in_fds[0]); close(out_fds[1]);
  write(in_fds[1], "dash stdin\n", 11);
  close(in_fds[1]);
  size_t tot = 0; ssize_t n;
  while (tot < sizeof(buf)-1 && (n = read(out_fds[0], buf+tot, sizeof(buf)-1-tot)) > 0)
    tot += n;
  buf[tot] = '\0';
  close(out_fds[0]);
  int st; waitpid(pid, &st, 0);
  result("cat - reads stdin", strcmp(buf, "dash stdin\n") == 0);

  /* cat empty file outputs nothing */
  write_file("/mnt/tmp/tb_cat_empty", "");
  char *a1[] = { "/bin/cat", "/mnt/tmp/tb_cat_empty", NULL };
  run_capture(a1, buf, sizeof(buf));
  result("cat empty file outputs nothing", strcmp(buf, "") == 0);
  unlink("/mnt/tmp/tb_cat_empty");
}

/* -----------------------------------------------------------------------
 * ls — stat-based (single file, missing, -l)
 * -------------------------------------------------------------------- */
static void test_ls_extra(void) {
  printf("Test: ls (extra)\n");
  char buf[1024];

  /* ls single file prints its name */
  write_file("/mnt/tmp/tb_ls_single", "x");
  char *a1[] = { "/bin/ls", "/mnt/tmp/tb_ls_single", NULL };
  run_capture(a1, buf, sizeof(buf));
  /* strip newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("ls single file prints path", strcmp(buf, "/mnt/tmp/tb_ls_single") == 0);

  /* ls -l single file shows permissions */
  char *a2[] = { "/bin/ls", "-l", "/mnt/tmp/tb_ls_single", NULL };
  run_capture(a2, buf, sizeof(buf));
  result("ls -l single file: starts with -", buf[0] == '-');
  result("ls -l single file: shows name", strstr(buf, "tb_ls_single") != NULL);
  unlink("/mnt/tmp/tb_ls_single");

  /* ls on nonexistent exits 1 */
  char *a3[] = { "/bin/ls", "/mnt/tmp/no_such_ls_xyz", NULL };
  int s = run_exit(a3);
  result("ls nonexistent exits 1", WIFEXITED(s) && WEXITSTATUS(s) == 1);

  /* ls multiple paths: one valid one invalid — exits nonzero */
  write_file("/mnt/tmp/tb_ls_multi", "x");
  char *a4[] = { "/bin/ls", "/mnt/tmp/tb_ls_multi", "/mnt/tmp/no_such_ls_xyz", NULL };
  s = run_exit(a4);
  result("ls mixed paths exits non-zero", WIFEXITED(s) && WEXITSTATUS(s) != 0);
  unlink("/mnt/tmp/tb_ls_multi");
}

int main(void) {
  printf("=== bin tests ===\n");

  /* Ensure /mnt is mounted — may already be mounted by rc, ignore error. */
  mount("", "/mnt", "sbfs", 0, NULL);
  mkdir("/mnt/tmp", 0777);

  test_echo();
  test_echo_extra();
  test_true_false();
  test_pwd();
  test_wc();
  test_wc_pipe_echo();
  test_wc_edge();
  test_kill();
  test_ps();
  test_test();
  test_test_extra();
  test_fs_basics();
  test_touch();
  test_cat();
  test_cat_extra();
  test_cp_cat();
  test_cp_extra();
  test_cp_recursive();
  test_mv();
  test_mv_dir();
  test_rm_extra();
  test_rm_force();
  test_chmod();
  test_chmod_extra();
  test_ln_readlink();
  test_ls();
  test_ls_extra();
  test_tail();
  test_tail_extra();
  test_head();
  test_date();
  test_df();
  test_dmesg();
  test_sleep();
  test_env();
  test_new_options();
  test_printf();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
