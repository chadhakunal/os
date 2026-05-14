#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h> 

#define COMMAND_BUF_SIZE 256

// Global variables for script arguments
static char **script_argv = NULL;
static int script_argc = 0;

void parse_and_exec(const char *buf);

/* Expand * to list all files in current directory */
void expand_glob(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0;
  size_t out_pos = 0;

  while (input[in_pos] != '\0' && out_pos < output_size - 1) {
    if (input[in_pos] == '*') {
      // Expand * to all files in current directory
      int fd = open(".", O_RDONLY);
      if (fd >= 0) {
        struct dirent entries[64];
        int n = getdents(fd, entries, sizeof(entries));
        if (n > 0) {
          int num_entries = n / sizeof(struct dirent);
          bool first = true;

          for (int i = 0; i < num_entries; i++) {
            // Skip . and ..
            if (strcmp(entries[i].d_name, ".") == 0 || strcmp(entries[i].d_name, "..") == 0) {
              continue;
            }

            // Add space before entry if not the first
            if (!first && out_pos < output_size - 1) {
              output[out_pos++] = ' ';
            }
            first = false;

            // Copy the filename
            const char *name = entries[i].d_name;
            size_t name_len = strlen(name);
            for (size_t j = 0; j < name_len && out_pos < output_size - 1; j++) {
              output[out_pos++] = name[j];
            }
          }
        }
        close(fd);
      }
      in_pos++;
    } else {
      // Regular character, just copy it
      output[out_pos++] = input[in_pos++];
    }
  }

  output[out_pos] = '\0';
}

/* Expand $0, $1, $2, etc. in the input string with script arguments */
void expand_args(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0;
  size_t out_pos = 0;

  while (input[in_pos] != '\0' && out_pos < output_size - 1) {
    if (input[in_pos] == '$' && input[in_pos + 1] >= '0' && input[in_pos + 1] <= '9') {
      // Found $N where N is a digit
      int arg_num = input[in_pos + 1] - '0';
      in_pos += 2;

      // Replace with the corresponding argument if it exists
      if (arg_num < script_argc && script_argv[arg_num] != NULL) {
        const char *arg = script_argv[arg_num];
        size_t arg_len = strlen(arg);

        // Copy the argument value
        for (size_t i = 0; i < arg_len && out_pos < output_size - 1; i++) {
          output[out_pos++] = arg[i];
        }
      }
      // If argument doesn't exist, just skip the $N (replace with empty string)
    } else {
      // Regular character, just copy it
      output[out_pos++] = input[in_pos++];
    }
  }

  output[out_pos] = '\0';
}

int main(int argc, char **argv, char **envp) {
  // Handle -c option: sh -c "command string"
  if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
    const char *command = argv[2];

    // Set global script arguments for $0, $1, etc. expansion
    // In -c mode: $0 = sh, $1 = -c, $2 = command, etc.
    script_argv = argv;
    script_argc = argc;

    char expanded[1024];
    char glob_expanded[1024];

    // Expand arguments and globs
    expand_args(command, expanded, sizeof(expanded));
    expand_glob(expanded, glob_expanded, sizeof(glob_expanded));
    parse_and_exec(glob_expanded);

    return 0;
  }

  // If a script file is provided as argv[1], execute it and exit
  if (argc >= 2) {
    const char *script_path = argv[1];

    // Set global script arguments for $0, $1, etc. expansion
    // $0 = script path, $1 = first arg to script, etc.
    script_argv = argv;
    script_argc = argc;

    int fd = open(script_path, O_RDONLY);
    if (fd < 0) {
      printf("sh: cannot open script '%s'\n", script_path);
      return 1;
    }

    char buf[1024];
    char expanded[1024];
    char glob_expanded[1024];
    ssize_t n;
    size_t line_start = 0;
    size_t total_read = 0;

    // Read and execute the script line by line
    while ((n = read(fd, buf + total_read, sizeof(buf) - total_read - 1)) > 0) {
      total_read += n;
      buf[total_read] = '\0';

      // Process complete lines
      for (size_t i = line_start; i < total_read; i++) {
        if (buf[i] == '\n') {
          buf[i] = '\0';
          if (line_start < i && buf[line_start] != '#') {
            // Expand $0, $1, etc. first
            expand_args(&buf[line_start], expanded, sizeof(expanded));
            // Then expand * for glob
            expand_glob(expanded, glob_expanded, sizeof(glob_expanded));
            parse_and_exec(glob_expanded);
          }
          line_start = i + 1;
        }
      }

      // Move any incomplete line to the start of the buffer
      if (line_start < total_read) {
        size_t remaining = total_read - line_start;
        for (size_t i = 0; i < remaining; i++) {
          buf[i] = buf[line_start + i];
        }
        total_read = remaining;
        line_start = 0;
      } else {
        total_read = 0;
        line_start = 0;
      }
    }

    // Execute any remaining line
    if (total_read > 0 && buf[0] != '#') {
      buf[total_read] = '\0';
      expand_args(buf, expanded, sizeof(expanded));
      expand_glob(expanded, glob_expanded, sizeof(glob_expanded));
      parse_and_exec(glob_expanded);
    }

    close(fd);
    return 0;
  }

  // Interactive mode - no script provided
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
  char glob_expanded[1024];
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
        // Expand * for glob in interactive mode too
        expand_glob(buf, glob_expanded, sizeof(glob_expanded));
        parse_and_exec(glob_expanded);
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

  // Skip leading quotes if present
  if (buf[i] == '"') {
    i++;
  }

  // Parse command
  while (i < COMMAND_BUF_SIZE - 1 && buf[i] != ' ' && buf[i] != '\0' && buf[i] != '"') {
    command_buf[cmd_len] = buf[i];
    i++;
    cmd_len++;
  }
  command_buf[cmd_len] = '\0';

  // Skip trailing quote if present
  if (buf[i] == '"') {
    i++;
  }

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
