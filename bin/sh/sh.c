#include <stdio.h>
#include <unistd.h>
#include <types.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define COMMAND_BUF_SIZE  256
#define MAX_HISTORY       128
#define HISTORY_FILE      "/.history"

/* -------------------------------------------------------------------------
 * History subsystem
 * ---------------------------------------------------------------------- */
static char history[MAX_HISTORY][COMMAND_BUF_SIZE];
static int  history_count = 0;

static void history_load(void) {
  int fd = open(HISTORY_FILE, O_RDONLY);
  if (fd < 0) return;

  static char buf[MAX_HISTORY * COMMAND_BUF_SIZE];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  char *p = buf;
  char *end = buf + n;
  while (p < end && history_count < MAX_HISTORY) {
    char *nl = p;
    while (nl < end && *nl != '\n') nl++;
    int len = nl - p;
    if (len > 0 && len < COMMAND_BUF_SIZE) {
      memcpy(history[history_count], p, len);
      history[history_count][len] = '\0';
      history_count++;
    }
    p = nl + 1;
  }
}

static void history_push(const char *cmd) {
  if (cmd[0] == '\0') return;
  /* Don't add if same as last entry. */
  if (history_count > 0 &&
      strcmp(history[history_count - 1], cmd) == 0)
    return;

  if (history_count < MAX_HISTORY) {
    strncpy(history[history_count], cmd, COMMAND_BUF_SIZE - 1);
    history[history_count][COMMAND_BUF_SIZE - 1] = '\0';
    history_count++;
  } else {
    /* Shift oldest out. */
    for (int i = 0; i < MAX_HISTORY - 1; i++)
      memcpy(history[i], history[i + 1], COMMAND_BUF_SIZE);
    strncpy(history[MAX_HISTORY - 1], cmd, COMMAND_BUF_SIZE - 1);
    history[MAX_HISTORY - 1][COMMAND_BUF_SIZE - 1] = '\0';
  }

  /* Append to history file. */
  int fd = open(HISTORY_FILE, O_WRONLY | O_CREAT);
  if (fd >= 0) {
    lseek(fd, 0, SEEK_END);
    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);
    close(fd);
  }
}

/* -------------------------------------------------------------------------
 * Line editor — char-by-char readline with history navigation.
 * Requires the TTY to be in raw mode (TCSRAW) before calling.
 * Returns the number of bytes placed in out_buf (not counting '\0').
 * ---------------------------------------------------------------------- */

static void term_erase(int n) {
  for (int i = 0; i < n; i++)
    write(1, "\b \b", 3);
}

static int readline_with_history(const char *prompt, char *out_buf,
                                  int max_len) {
  char line[COMMAND_BUF_SIZE];
  int  line_len = 0;

  char saved_line[COMMAND_BUF_SIZE] = {0};
  int  hist_pos = history_count;

  write(1, prompt, strlen(prompt));

  while (1) {
    char c;
    if (read(0, &c, 1) <= 0) continue;

    if (c == '\n' || c == '\r') {
      write(1, "\n", 1);
      line[line_len] = '\0';
      memcpy(out_buf, line, line_len + 1);
      return line_len;
    }

    if (c == 0x7f || c == 0x08) {
      if (line_len > 0) { line_len--; term_erase(1); }
      continue;
    }

    if (c == '\x1b') {
      char seq[2];
      if (read(0, &seq[0], 1) <= 0) continue;
      if (read(0, &seq[1], 1) <= 0) continue;
      if (seq[0] != '[') continue;

      if (seq[1] == 'A') {
        /* Up arrow — older history entry. */
        if (hist_pos == 0) continue;
        if (hist_pos == history_count)
          memcpy(saved_line, line, line_len + 1);
        hist_pos--;
        term_erase(line_len);
        line_len = strlen(history[hist_pos]);
        memcpy(line, history[hist_pos], line_len + 1);
        write(1, line, line_len);
      } else if (seq[1] == 'B') {
        /* Down arrow — newer entry or restore live input. */
        if (hist_pos == history_count) continue;
        hist_pos++;
        term_erase(line_len);
        if (hist_pos == history_count) {
          line_len = strlen(saved_line);
          memcpy(line, saved_line, line_len + 1);
        } else {
          line_len = strlen(history[hist_pos]);
          memcpy(line, history[hist_pos], line_len + 1);
        }
        write(1, line, line_len);
      }
      continue;
    }

    if (c >= 0x20 && line_len < max_len - 1) {
      line[line_len++] = c;
      write(1, &c, 1);
    }
  }
}

/* -------------------------------------------------------------------------
 * Builtins
 * ---------------------------------------------------------------------- */

void parse_and_exec(const char *buf);

static void echo_write_arg(int fd, const char *s) {
  size_t len = strlen(s);
  if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
    if (len > 2) write(fd, s + 1, len - 2);
  } else {
    write(fd, s, len);
  }
}

static int builtin_echo(int argc, char *argv[], int out_fd) {
  for (int i = 1; i < argc; i++) {
    if (i > 1) write(out_fd, " ", 1);
    echo_write_arg(out_fd, argv[i]);
  }
  write(out_fd, "\n", 1);
  return 0;
}

static int builtin_pwd(void) {
  char buf[256];
  if (getcwd(buf, sizeof(buf)) != NULL)
    printf("%s\n", buf);
  else
    printf("pwd: error\n");
  return 0;
}

static int builtin_cd(int argc, char *argv[]) {
  const char *path = (argc < 2) ? "/" : argv[1];
  if (chdir(path) < 0) {
    printf("cd: cannot change directory to '%s'\n", path);
    return 1;
  }
  return 0;
}

static int builtin_history(void) {
  for (int i = 0; i < history_count; i++)
    printf("  %d  %s\n", i + 1, history[i]);
  return 0;
}

/* -------------------------------------------------------------------------
 * Command parser / executor
 * ---------------------------------------------------------------------- */

void parse_and_exec(const char *buf) {
  char command_buf[COMMAND_BUF_SIZE];
  char *argv[16];
  int argc = 0;

  size_t i = 0;
  size_t cmd_len = 0;

  while (i < COMMAND_BUF_SIZE - 1 && buf[i] != ' ' && buf[i] != '\0')
    command_buf[cmd_len++] = buf[i++];
  command_buf[cmd_len] = '\0';

  if (cmd_len == 0) return;

  char full_path[256];
  if (command_buf[0] == '/') {
    strncpy(full_path, command_buf, sizeof(full_path) - 1);
    full_path[sizeof(full_path) - 1] = '\0';
  } else {
    /* Prepend /bin/ */
    full_path[0] = '/'; full_path[1] = 'b'; full_path[2] = 'i';
    full_path[3] = 'n'; full_path[4] = '/';
    for (size_t j = 0; j < cmd_len && j < 250; j++)
      full_path[5 + j] = command_buf[j];
    full_path[5 + cmd_len] = '\0';
  }

  argv[argc++] = full_path;

  while (buf[i] != '\0' && argc < 15) {
    while (buf[i] == ' ') i++;
    if (buf[i] == '\0') break;
    argv[argc++] = (char *)&buf[i];
    while (buf[i] != ' ' && buf[i] != '\0') i++;
    if (buf[i] == ' ') { ((char *)buf)[i] = '\0'; i++; }
  }
  argv[argc] = NULL;

  /* Scan for output redirection '>'. */
  const char *redirect_out = NULL;
  for (int ri = 1; ri < argc - 1; ri++) {
    if (argv[ri][0] == '>' && argv[ri][1] == '\0') {
      redirect_out = argv[ri + 1];
      for (int rj = ri; rj < argc - 2; rj++)
        argv[rj] = argv[rj + 2];
      argc -= 2;
      argv[argc] = NULL;
      break;
    }
  }

  /* Builtins */
  if (strcmp("echo", command_buf) == 0) {
    int out_fd = 1;
    if (redirect_out != NULL) {
      out_fd = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC);
      if (out_fd < 0) { printf("sh: cannot open '%s'\n", redirect_out); return; }
    }
    builtin_echo(argc, argv, out_fd);
    if (redirect_out != NULL) close(out_fd);
    return;
  }
  if (strcmp("cd",      command_buf) == 0) { builtin_cd(argc, argv);  return; }
  if (strcmp("pwd",     command_buf) == 0) { builtin_pwd();            return; }
  if (strcmp("history", command_buf) == 0) { builtin_history();        return; }
  if (strcmp("exit",    command_buf) == 0) { exit(0); }

  /* External command — switch to canonical so the child gets a normal TTY. */
  ioctl(0, TCSCANON, (void*)0);

  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    if (redirect_out != NULL) {
      int fd = open(redirect_out, O_WRONLY | O_CREAT | O_TRUNC);
      if (fd < 0) { printf("sh: cannot open '%s'\n", redirect_out); exit(1); }
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

  /* Restore raw mode for our own readline. */
  ioctl(0, TCSRAW, (void*)0);
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int main(int argc, char **argv, char **envp) {
  (void)argc; (void)argv; (void)envp;

  pid_t shell_pgid = getpid();
  tcsetpgrp(0, shell_pgid);

  history_load();
  ioctl(0, TCSRAW, (void*)0);

  char buf[COMMAND_BUF_SIZE];
  char cwd[256];
  char prompt[300];

  while (1) {
    if (getcwd(cwd, sizeof(cwd)) == NULL)
      cwd[0] = '\0';

    /* Build prompt string. */
    int pi = 0;
    for (int j = 0; cwd[j] && pi < 280; j++) prompt[pi++] = cwd[j];
    prompt[pi++] = ' '; prompt[pi++] = '$'; prompt[pi++] = ' ';
    prompt[pi] = '\0';

    int n = readline_with_history(prompt, buf, sizeof(buf));
    if (n > 0) {
      history_push(buf);
      parse_and_exec(buf);
    }
  }

  return 0;
}
