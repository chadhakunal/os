#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> 

#define COMMAND_BUF_SIZE 256

void parse_and_exec(const char *buf);

int main(int argc, char **argv, char **envp) {
  printf("Shell started!\n");
  printf("argc = %d\n", argc);

  printf("Arguments:\n");
  for (int i = 0; i < argc; i++) {
    printf("  argv[%d] = %s\n", i, argv[i]);
  }

  printf("Environment:\n");
  printf("envp pointer = %p\n", envp);
  if (envp != NULL) {
    printf("envp[0] = %p\n", envp[0]);
    for (int i = 0; envp[i] != NULL; i++) {
      printf("  envp[%d] = %s\n", i, envp[i]);
    }
  }

  pid_t shell_pgid = getpid();
  tcsetpgrp(0, shell_pgid);

  char buf[1024];
  char cwd[256];
  while (true) {
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s $ ", cwd);
    } else {
      printf("$ ");
    }
    ssize_t n = read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = '\0';

      if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
      }

      if (buf[0] != '\0') {
        parse_and_exec(buf);
      }
    }
  }

  return 0;
}

/* Write a string to fd, stripping one layer of surrounding "..." quotes. */
static void echo_write_arg(int fd, const char *s) {
  size_t len = strlen(s);
  if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
    if (len > 2)
      write(fd, s + 1, len - 2);
    /* empty string: nothing to write before the newline */
  } else {
    write(fd, s, len);
  }
}

int echo(int argc, char *argv[], int out_fd) {
  for (int i = 1; i < argc; i++) {
    if (i > 1)
      write(out_fd, " ", 1);
    echo_write_arg(out_fd, argv[i]);
  }
  write(out_fd, "\n", 1);
  return 0;
}

int pwd(int argc, char *argv[]) {
  char buf[256];

  char *result = getcwd(buf, sizeof(buf));
  if (result == NULL) {
    printf("pwd: error getting current directory\n");
    return 1;
  }

  printf("%s\n", buf);
  return 0;
}

int cd(int argc, char *argv[]) {
  const char *path;

  if (argc < 2) {
    path = "/"; // When we do cd
  } else {
    path = argv[1]; // when we do cd x
  }

  if (chdir(path) < 0) {
    printf("cd: cannot change directory to '%s'\n", path);
    return 1;
  }

  return 0;
}

void parse_and_exec(const char *buf) {
  char command_buf[COMMAND_BUF_SIZE];
  char *argv[16];
  int argc = 0;

  size_t i = 0;
  size_t cmd_len = 0;

  // Parse command
  while (i < COMMAND_BUF_SIZE - 1 && buf[i] != ' ' && buf[i] != '\0') {
    command_buf[cmd_len] = buf[i];
    i++;
    cmd_len++;
  }
  command_buf[cmd_len] = '\0';

  if (cmd_len == 0) {
    return;
  }

  char full_path[256];

  if (command_buf[0] == '/') {
    for (size_t j = 0; j < cmd_len && j < 255; j++) {
      full_path[j] = command_buf[j];
    }
    full_path[cmd_len] = '\0';
  } else {
    full_path[0] = '/';
    full_path[1] = 'b';
    full_path[2] = 'i';
    full_path[3] = 'n';
    full_path[4] = '/';
    for (size_t j = 0; j < cmd_len && j < 250; j++) {
      full_path[5 + j] = command_buf[j];
    }
    full_path[5 + cmd_len] = '\0';
  }

  argv[argc++] = full_path;

  while (buf[i] != '\0' && argc < 15) {
    while (buf[i] == ' ') {
      i++;
    }

    if (buf[i] == '\0') {
      break;
    }

    argv[argc++] = (char *)&buf[i];

    while (buf[i] != ' ' && buf[i] != '\0') {
      i++;
    }

    if (buf[i] == ' ') {
      ((char *)buf)[i] = '\0';
      i++;
    }
  }

  argv[argc] = NULL;

  /* Scan for output redirection: pull '>' and its target out of argv. */
  const char *redirect_out = NULL;
  for (int ri = 1; ri < argc - 1; ri++) {
    if (argv[ri][0] == '>' && argv[ri][1] == '\0') {
      redirect_out = argv[ri + 1];
      /* Collapse the two slots out of argv. */
      for (int rj = ri; rj < argc - 2; rj++)
        argv[rj] = argv[rj + 2];
      argc -= 2;
      argv[argc] = NULL;
      break;
    }
  }

  if (strcmp("echo", command_buf) == 0) {
    int out_fd = 1; /* stdout */
    if (redirect_out != NULL) {
      out_fd = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC);
      if (out_fd < 0) {
        printf("sh: cannot open '%s' for writing\n", redirect_out);
        return;
      }
    }
    echo(argc, argv, out_fd);
    if (redirect_out != NULL)
      close(out_fd);
    return;
  } else if (strcmp("cd", command_buf) == 0) {
    cd(argc, argv);
    return;
  } else if (strcmp("pwd", command_buf) == 0) {
    pwd(argc, argv);
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    if (redirect_out != NULL) {
      int fd = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC);
      if (fd < 0) {
        printf("sh: cannot open '%s' for writing\n", redirect_out);
        exit(1);
      }
      dup2(fd, 1);
      close(fd);
    }
    char *envp[] = { NULL };
    execve(full_path, argv, envp);
    printf("sh: failed to execute %s\n", command_buf);
    exit(1);
  } else if (pid > 0) {
    setpgid(pid, pid);
    tcsetpgrp(0, pid);
    int status;
    wait(&status);
    tcsetpgrp(0, getpid());
  } else {
    printf("sh: fork failed\n");
  }
}
