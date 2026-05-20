#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

int main(void) {
  printf("=== waitpid exit status tests ===\n");
  int passed = 0, failed = 0;

  /* Test each exit code 0-3 directly via fork */
  for (int code = 0; code <= 3; code++) {
    pid_t pid = fork();
    if (pid == 0)
      _exit(code);

    int status = 0xdeadbeef;
    int ret = waitpid(pid, &status, 0);

    printf("exit(%d): waitpid ret=%d status=0x%x WIFEXITED=%d WEXITSTATUS=%d",
           code, ret, (unsigned)status,
           WIFEXITED(status), WEXITSTATUS(status));

    int ok = (ret == pid) && WIFEXITED(status) && WEXITSTATUS(status) == code;
    printf(" -> %s\n", ok ? "[PASS]" : "[FAIL]");
    if (ok) passed++; else failed++;
  }

  /* Test waitpid with a child that exits 1 via execve (like run_capture does) */
  printf("\nexecve exit status tests:\n");

  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
    char *argv[] = { "/bin/false", NULL };
    execve("/bin/false", argv, NULL);
    _exit(127);
  }
  close(fds[1]);
  char buf[4]; ssize_t n;
  while ((n = read(fds[0], buf, sizeof(buf))) > 0);
  close(fds[0]);

  int status = 0xdeadbeef;
  int ret = waitpid(pid, &status, 0);
  printf("execve /bin/false: waitpid ret=%d status=0x%x WIFEXITED=%d WEXITSTATUS=%d",
         ret, (unsigned)status, WIFEXITED(status), WEXITSTATUS(status));
  int ok = (ret == pid) && WIFEXITED(status) && WEXITSTATUS(status) == 1;
  printf(" -> %s\n", ok ? "[PASS]" : "[FAIL]");
  if (ok) passed++; else failed++;

  /* Same but without pipe (no dup2) */
  pid = fork();
  if (pid == 0) {
    char *argv[] = { "/bin/false", NULL };
    execve("/bin/false", argv, NULL);
    _exit(127);
  }
  status = 0xdeadbeef;
  ret = waitpid(pid, &status, 0);
  printf("execve /bin/false (no pipe): waitpid ret=%d status=0x%x WIFEXITED=%d WEXITSTATUS=%d",
         ret, (unsigned)status, WIFEXITED(status), WEXITSTATUS(status));
  ok = (ret == pid) && WIFEXITED(status) && WEXITSTATUS(status) == 1;
  printf(" -> %s\n", ok ? "[PASS]" : "[FAIL]");
  if (ok) passed++; else failed++;

  /* Simulated run_capture — capture stdout to see if test prints its argc/argv */
  printf("\nsimulated run_capture for /bin/test -f /no/such/file:\n");
  {
    int fds2[2];
    pipe(fds2);
    pid_t pid2 = fork();
    if (pid2 == 0) {
      close(fds2[0]);
      dup2(fds2[1], 1);
      close(fds2[1]);
      char *argv[] = { "/bin/test", "-f", "/no/such/file", NULL };
      execve("/bin/test", argv, NULL);
      _exit(127);
    }
    close(fds2[1]);
    char buf2[64]; ssize_t n2;
    size_t tot = 0;
    while (tot < sizeof(buf2)-1 && (n2 = read(fds2[0], buf2+tot, sizeof(buf2)-1-tot)) > 0)
      tot += n2;
    buf2[tot] = '\0';
    close(fds2[0]);
    printf("stdout: '%s'\n", buf2);

    int status2 = 0xdeadbeef;
    int ret2 = waitpid(pid2, &status2, 0);
    printf("waitpid ret=%d status=0x%x WIFEXITED=%d WEXITSTATUS=%d -> %s\n",
           ret2, (unsigned)status2,
           WIFEXITED(status2), WEXITSTATUS(status2),
           (WIFEXITED(status2) && WEXITSTATUS(status2) == 1) ? "[PASS]" : "[FAIL]");
    if (WIFEXITED(status2) && WEXITSTATUS(status2) == 1) passed++; else failed++;
  }

  /* Same but print argc inside test by using a debug wrapper — use /bin/echo instead */
  printf("\nrun_capture /bin/test -f /etc/rc (should be TRUE, exit 0):\n");
  {
    int fds3[2];
    pipe(fds3);
    pid_t pid3 = fork();
    if (pid3 == 0) {
      close(fds3[0]);
      dup2(fds3[1], 1);
      close(fds3[1]);
      char *argv[] = { "/bin/test", "-f", "/etc/rc", NULL };
      execve("/bin/test", argv, NULL);
      _exit(127);
    }
    close(fds3[1]);
    char buf3[64]; ssize_t n3;
    size_t tot3 = 0;
    while (tot3 < sizeof(buf3)-1 && (n3 = read(fds3[0], buf3+tot3, sizeof(buf3)-1-tot3)) > 0)
      tot3 += n3;
    buf3[tot3] = '\0';
    close(fds3[0]);
    int status3 = 0xdeadbeef;
    int ret3 = waitpid(pid3, &status3, 0);
    printf("waitpid ret=%d status=0x%x WEXITSTATUS=%d -> %s\n",
           ret3, (unsigned)status3, WEXITSTATUS(status3),
           (WIFEXITED(status3) && WEXITSTATUS(status3) == 0) ? "[PASS]" : "[FAIL]");
    if (WIFEXITED(status3) && WEXITSTATUS(status3) == 0) passed++; else failed++;
  }

  /* Test what stat() returns for missing vs existing file in this process */
  printf("\nstat() sanity in this process:\n");
  {
    struct stat st;
    int r1 = stat("/no/such/file", &st);
    int r2 = stat("/etc/rc", &st);
    printf("stat('/no/such/file') = %d (expect -1)\n", r1);
    printf("stat('/etc/rc')       = %d (expect 0)\n",  r2);
  }

  /* Run a child that calls stat and exits based on result */
  printf("\nchild stat exit code test:\n");
  {
    pid_t p = fork();
    if (p == 0) {
      struct stat st;
      int r = stat("/no/such/file", &st);
      /* exit 0 if stat succeeded (wrong), exit 1 if failed (correct) */
      _exit(r == 0 ? 0 : 1);
    }
    int st = 0; waitpid(p, &st, 0);
    printf("child stat('/no/such/file') exit=%d (expect 1)\n", WEXITSTATUS(st));
    if (WIFEXITED(st) && WEXITSTATUS(st) == 1) passed++; else failed++;
  }

  /* Does /bin/test exit correctly when called with environ (not NULL)? */
  printf("\n/bin/test with environ (not NULL envp):\n");
  {
    extern char **environ;
    int fds2[2];
    pipe(fds2);
    pid_t p = fork();
    if (p == 0) {
      close(fds2[0]);
      dup2(fds2[1], 1);
      close(fds2[1]);
      char *argv[] = { "/bin/test", "-f", "/no/such/file", NULL };
      execve("/bin/test", argv, environ);
      _exit(127);
    }
    close(fds2[1]);
    char buf[4]; ssize_t n;
    while ((n = read(fds2[0], buf, sizeof(buf))) > 0);
    close(fds2[0]);
    int st = 0xdeadbeef; waitpid(p, &st, 0);
    printf("status=0x%x WEXITSTATUS=%d -> %s\n", (unsigned)st, WEXITSTATUS(st),
           (WIFEXITED(st) && WEXITSTATUS(st) == 1) ? "[PASS]" : "[FAIL]");
    if (WIFEXITED(st) && WEXITSTATUS(st) == 1) passed++; else failed++;
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
