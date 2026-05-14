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

void test_input_redirection() {
  // Test reading from existing file via redirection
  pid_t pid = fork();
  if (pid == 0) {
    // Child: redirect stdin from /etc/rc
    int fd = open("/etc/rc", O_RDONLY);
    if (fd < 0) {
      exit(1);
    }
    dup2(fd, 0);
    close(fd);

    // Read from stdin (should read from /etc/rc)
    char buffer[16];
    ssize_t n = read(0, buffer, sizeof(buffer));

    // Check if it starts with shebang
    int success = (n >= 2 && buffer[0] == '#' && buffer[1] == '!');
    exit(success ? 0 : 1);
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("dup2 redirects stdin correctly", status == 0);
  } else {
    test_result("fork for stdin redirect test", 0);
  }
}

void test_output_redirection_dup2() {
  // Test that dup2 works for stdout (we can't verify the output without writable fs)
  pid_t pid = fork();
  if (pid == 0) {
    // Child: try to dup2 stdout to stderr
    int ret = dup2(2, 1);
    exit(ret >= 0 ? 0 : 1);
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("dup2 works for file descriptors", status == 0);
  } else {
    test_result("fork for dup2 test", 0);
  }
}

void test_exec_replaces_process() {
  // Test that exec actually replaces the process
  pid_t parent_pid = getpid();

  pid_t pid = fork();
  if (pid == 0) {
    // Child: exec should replace this process
    pid_t my_pid = getpid();
    char *argv[] = {"/bin/ls", NULL};
    char *envp[] = {NULL};

    execve("/bin/ls", argv, envp);

    // If we reach here, exec failed
    printf("  [ERROR] exec did not replace process (still at pid %d)\n", my_pid);
    exit(1);
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("exec replaces process", status == 0);
  } else {
    test_result("fork for exec test", 0);
  }
}

void test_exec_with_args() {
  // Test exec with arguments
  pid_t pid = fork();
  if (pid == 0) {
    char *argv[] = {"/bin/cat", "/etc/rc", NULL};
    char *envp[] = {NULL};
    execve("/bin/cat", argv, envp);
    exit(1); // Should not reach
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("exec passes arguments correctly", status == 0);
  } else {
    test_result("fork for exec with args", 0);
  }
}

void test_shell_exec_builtin() {
  // Test the shell's exec builtin via sh -c
  pid_t pid = fork();
  if (pid == 0) {
    char *argv[] = {"/bin/sh", "-c", "exec ls", NULL};
    char *envp[] = {NULL};
    execve("/bin/sh", argv, envp);
    exit(1); // Should not reach
  } else if (pid > 0) {
    int status;
    wait(&status);
    test_result("shell exec builtin works", status == 0);
  } else {
    test_result("fork for shell exec test", 0);
  }
}

void test_open_flags() {
  // Test that O_RDONLY works
  int fd = open("/etc/rc", O_RDONLY);
  test_result("O_RDONLY opens existing file", fd >= 0);
  if (fd >= 0) close(fd);

  // Test that opening non-existent file fails
  fd = open("/nonexistent", O_RDONLY);
  test_result("O_RDONLY fails for non-existent file", fd < 0);
  if (fd >= 0) close(fd);
}

int main(int argc, char **argv) {
  printf("\n=== Shell Features Tests ===\n\n");

  printf("File Descriptor Operations:\n");
  test_open_flags();
  test_input_redirection();
  test_output_redirection_dup2();

  printf("\nExec Functionality:\n");
  test_exec_replaces_process();
  test_exec_with_args();
  test_shell_exec_builtin();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", test_passed);
  printf("Failed: %d\n", test_failed);
  printf("\n");

  return test_failed > 0 ? 1 : 0;
}
