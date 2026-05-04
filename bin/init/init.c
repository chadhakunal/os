#include <stdio.h>
#include <unistd.h>
#include <types.h>

int main() {
  pid_t pid = fork();
  if (pid == 0) {
    printf("Child! yielding then I'm killing myself!\n");
    char buf[1024];
    size_t read_bytes = read(0, buf, sizeof(buf));
    buf[read_bytes-1] = '\0';
    printf("Read %d bytes\n");
    printf(buf);
    return 0;
  } else {
    printf("Parent! I'm waiting for %llu\n", pid);
    int wstatus;
    wait(&wstatus);
    printf("Parent! Woke up child has been killed, exiting...\n");
  }
  return 0;
}
