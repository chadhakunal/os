#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
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

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed != 0;
}
