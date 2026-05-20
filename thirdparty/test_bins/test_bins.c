#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

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
    execve(argv[0], argv, NULL);
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
  char *t1[] = { "/bin/touch", "/tmp/tb_touch", NULL };
  result("touch creates file", child_ok(run_exit(t1)));
  result("touch: file exists after touch", stat("/tmp/tb_touch", &(struct stat){}) == 0);

  /* rm removes it */
  char *r1[] = { "/bin/rm", "/tmp/tb_touch", NULL };
  result("rm removes file", child_ok(run_exit(r1)));
  result("rm: file gone after rm", stat("/tmp/tb_touch", &(struct stat){}) != 0);

  /* mkdir creates a dir */
  char *m1[] = { "/bin/mkdir", "/tmp/tb_dir", NULL };
  result("mkdir creates dir", child_ok(run_exit(m1)));
  struct stat st;
  result("mkdir: is a directory", stat("/tmp/tb_dir", &st) == 0 && S_ISDIR(st.st_mode));

  /* rmdir removes it */
  char *rd1[] = { "/bin/rmdir", "/tmp/tb_dir", NULL };
  result("rmdir removes dir", child_ok(run_exit(rd1)));
  result("rmdir: dir gone", stat("/tmp/tb_dir", &st) != 0);

  /* mkdir -p creates nested dirs */
  char *m2[] = { "/bin/mkdir", "-p", "/tmp/tb_nest/a/b", NULL };
  result("mkdir -p nested", child_ok(run_exit(m2)));
  result("mkdir -p: leaf is dir", stat("/tmp/tb_nest/a/b", &st) == 0 && S_ISDIR(st.st_mode));

  /* cleanup */
  char *rd2[] = { "/bin/rmdir", "/tmp/tb_nest/a/b", NULL };
  char *rd3[] = { "/bin/rmdir", "/tmp/tb_nest/a", NULL };
  char *rd4[] = { "/bin/rmdir", "/tmp/tb_nest", NULL };
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
  write_file("/tmp/tb_src", "hello cp\n");

  /* cp it */
  char *cp1[] = { "/bin/cp", "/tmp/tb_src", "/tmp/tb_dst", NULL };
  result("cp exits 0", child_ok(run_exit(cp1)));

  /* cat it back */
  char *cat1[] = { "/bin/cat", "/tmp/tb_dst", NULL };
  run_capture(cat1, buf, sizeof(buf));
  result("cp: content preserved", strcmp(buf, "hello cp\n") == 0);

  /* cat from tarfs file */
  char *cat2[] = { "/bin/cat", "/etc/rc", NULL };
  int s = run_capture(cat2, buf, sizeof(buf));
  result("cat tarfs file exits 0", child_ok(s));
  result("cat tarfs file non-empty", strlen(buf) > 0);

  /* cleanup */
  unlink("/tmp/tb_src");
  unlink("/tmp/tb_dst");
}

/* -----------------------------------------------------------------------
 * mv
 * -------------------------------------------------------------------- */
static void test_mv(void) {
  printf("Test: mv\n");
  char buf[64];

  write_file("/tmp/tb_mv_src", "mv content\n");

  char *mv1[] = { "/bin/mv", "/tmp/tb_mv_src", "/tmp/tb_mv_dst", NULL };
  result("mv exits 0", child_ok(run_exit(mv1)));

  result("mv: src gone", stat("/tmp/tb_mv_src", &(struct stat){}) != 0);

  read_file("/tmp/tb_mv_dst", buf, sizeof(buf));
  result("mv: content at dst", strcmp(buf, "mv content\n") == 0);

  unlink("/tmp/tb_mv_dst");
}

/* -----------------------------------------------------------------------
 * chmod
 * -------------------------------------------------------------------- */
static void test_chmod(void) {
  printf("Test: chmod\n");

  write_file("/tmp/tb_chmod", "data");

  char *ch1[] = { "/bin/chmod", "755", "/tmp/tb_chmod", NULL };
  result("chmod exits 0", child_ok(run_exit(ch1)));

  struct stat st;
  stat("/tmp/tb_chmod", &st);
  result("chmod 755: exec bit set", (st.st_mode & 0111) != 0);

  char *ch2[] = { "/bin/chmod", "644", "/tmp/tb_chmod", NULL };
  run_exit(ch2);
  stat("/tmp/tb_chmod", &st);
  result("chmod 644: exec bit cleared", (st.st_mode & 0111) == 0);

  unlink("/tmp/tb_chmod");
}

/* -----------------------------------------------------------------------
 * ln (hard link) and readlink
 * -------------------------------------------------------------------- */
static void test_ln_readlink(void) {
  printf("Test: ln / readlink\n");
  char buf[256];

  /* Hard link within /tmp */
  write_file("/tmp/tb_hard_src", "hard link data\n");
  char *ln1[] = { "/bin/ln", "/tmp/tb_hard_src", "/tmp/tb_hard_dst", NULL };
  result("ln hard link exits 0", child_ok(run_exit(ln1)));
  read_file("/tmp/tb_hard_dst", buf, sizeof(buf));
  result("ln hard: content readable via link", strcmp(buf, "hard link data\n") == 0);
  unlink("/tmp/tb_hard_src");
  unlink("/tmp/tb_hard_dst");

  /* Symlink within /tmp pointing to a /tmp file */
  write_file("/tmp/tb_sym_target", "symlink target\n");
  char *ln2[] = { "/bin/ln", "-s", "/tmp/tb_sym_target", "/tmp/tb_sym_link", NULL };
  result("ln -s exits 0", child_ok(run_exit(ln2)));

  /* readlink shows the target */
  char *rl1[] = { "/bin/readlink", "/tmp/tb_sym_link", NULL };
  run_capture(rl1, buf, sizeof(buf));
  /* strip newline */
  size_t len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink shows target", strcmp(buf, "/tmp/tb_sym_target") == 0);

  /* stat follows symlink and sees the file */
  struct stat st;
  result("stat follows symlink", stat("/tmp/tb_sym_link", &st) == 0 && S_ISREG(st.st_mode));

  /* cat through symlink reads target content */
  char *cat1[] = { "/bin/cat", "/tmp/tb_sym_link", NULL };
  run_capture(cat1, buf, sizeof(buf));
  result("cat through symlink", strcmp(buf, "symlink target\n") == 0);

  unlink("/tmp/tb_sym_link");
  unlink("/tmp/tb_sym_target");

  /* Symlink in /tmp pointing across mount into tarfs (/etc/rc) */
  char *ln3[] = { "/bin/ln", "-s", "/etc/rc", "/tmp/tb_cross_link", NULL };
  result("ln -s cross-mount exits 0", child_ok(run_exit(ln3)));

  char *rl2[] = { "/bin/readlink", "/tmp/tb_cross_link", NULL };
  run_capture(rl2, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink cross-mount target", strcmp(buf, "/etc/rc") == 0);

  /* stat should follow the symlink into tarfs */
  result("stat cross-mount symlink", stat("/tmp/tb_cross_link", &st) == 0 && S_ISREG(st.st_mode));

  /* cat through cross-mount symlink */
  char *cat2[] = { "/bin/cat", "/tmp/tb_cross_link", NULL };
  int s = run_capture(cat2, buf, sizeof(buf));
  result("cat cross-mount symlink exits 0", child_ok(s));
  result("cat cross-mount symlink non-empty", strlen(buf) > 0);

  unlink("/tmp/tb_cross_link");

  /* Symlink in /tmp pointing to /bin (a directory on tarfs) */
  char *ln4[] = { "/bin/ln", "-s", "/bin", "/tmp/tb_bin_link", NULL };
  result("ln -s to dir exits 0", child_ok(run_exit(ln4)));
  result("stat symlink-to-dir: is dir", stat("/tmp/tb_bin_link", &st) == 0 && S_ISDIR(st.st_mode));
  unlink("/tmp/tb_bin_link");

  /* Relative symlink: link in /tmp pointing to ../etc/rc */
  char *ln5[] = { "/bin/ln", "-s", "../etc/rc", "/tmp/tb_rel_link", NULL };
  result("ln -s relative exits 0", child_ok(run_exit(ln5)));
  char *rl3[] = { "/bin/readlink", "/tmp/tb_rel_link", NULL };
  run_capture(rl3, buf, sizeof(buf));
  len = strlen(buf);
  if (len && buf[len-1] == '\n') buf[len-1] = '\0';
  result("readlink relative symlink", strcmp(buf, "../etc/rc") == 0);
  result("stat relative symlink resolves", stat("/tmp/tb_rel_link", &st) == 0 && S_ISREG(st.st_mode));
  unlink("/tmp/tb_rel_link");
}

/* -----------------------------------------------------------------------
 * tail
 * -------------------------------------------------------------------- */
static void test_tail(void) {
  printf("Test: tail\n");
  char buf[256];

  /* Write a 5-line file */
  write_file("/tmp/tb_tail", "line1\nline2\nline3\nline4\nline5\n");

  char *t1[] = { "/bin/tail", "-n", "2", "/tmp/tb_tail", NULL };
  run_capture(t1, buf, sizeof(buf));
  result("tail -n 2: last 2 lines", strcmp(buf, "line4\nline5\n") == 0);

  char *t2[] = { "/bin/tail", "-n", "1", "/tmp/tb_tail", NULL };
  run_capture(t2, buf, sizeof(buf));
  result("tail -n 1: last line", strcmp(buf, "line5\n") == 0);

  unlink("/tmp/tb_tail");
}

int main(void) {
  printf("=== bin tests ===\n");
  test_echo();
  test_true_false();
  test_pwd();
  test_wc();
  test_kill();
  test_ps();
  test_test();
  test_wc_pipe_echo();
  test_fs_basics();
  test_cp_cat();
  test_mv();
  test_chmod();
  test_ln_readlink();
  test_tail();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
