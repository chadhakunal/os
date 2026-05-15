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

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
