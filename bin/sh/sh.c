#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <types.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define COMMAND_BUF_SIZE  256
#define MAX_HISTORY       128
#define HISTORY_FILE      "/.history"
#define SCRIPT_BUF_SIZE   65536
#define MAX_SCRIPT_LINES  2048

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
    if (read(0, &c, 1) <= 0)
      continue;

    /* Ctrl-C: cancel current line, reshow prompt. */
    if (c == 0x03) {
      term_erase(line_len);
      write(1, "^C\n", 3);
      write(1, prompt, strlen(prompt));
      line_len = 0;
      continue;
    }

    /* Ctrl-Z: ignored at prompt (shell has SIG_IGN for SIGTSTP). */
    if (c == 0x1a) {
      write(1, "^Z\n", 3);
      write(1, prompt, strlen(prompt));
      continue;
    }

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

// Global variables for script arguments
static char **script_argv = NULL;
static int script_argc = 0;

int parse_and_exec(char *buf);

static void expand_args(const char *input, char *output, size_t output_size);
static void expand_glob(const char *input, char *output, size_t output_size);

static int trim_line(char *s) {
  char *start = s;
  while (*start == ' ' || *start == '\t' || *start == '\r')
    start++;
  if (start != s)
    memmove(s, start, strlen(start) + 1);
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r'))
    s[--len] = '\0';
  return (int)len;
}

static int builtin_test(int argc, char *argv[]) {
  int i = 1;
  int negate = 0;

  if (argc > 1 && strcmp(argv[1], "!") == 0) {
    negate = 1;
    i++;
  }
  if (i >= argc)
    return negate ? 0 : 1;

  if (strcmp(argv[i], "-x") == 0 && i + 1 < argc) {
    struct stat st;
    if (stat(argv[i + 1], &st) != 0)
      return negate ? 0 : 1;
    int ok = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
    struct stat st;
    int ok = stat(argv[i + 1], &st) == 0 && S_ISREG(st.st_mode);
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
    struct stat st;
    int ok = stat(argv[i + 1], &st) == 0 && S_ISDIR(st.st_mode);
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
    struct stat st;
    int ok = stat(argv[i + 1], &st) == 0;
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  return 1;
}

static int run_command_line(char *line) {
  char expanded[1024];
  char glob_expanded[1024];
  int last = 0;

  trim_line(line);
  if (line[0] == '\0' || line[0] == '#')
    return 0;

  expand_args(line, expanded, sizeof(expanded));
  expand_glob(expanded, glob_expanded, sizeof(glob_expanded));

  char *p = glob_expanded;
  while (*p) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '\0')
      break;
    char *start = p;
    while (*p && *p != ';')
      p++;
    char saved = *p;
    if (saved)
      *p++ = '\0';
    trim_line(start);
    if (start[0] != '\0')
      last = parse_and_exec(start);
  }
  return last;
}

static void parse_if_condition(const char *line, char *cond, size_t condsz) {
  const char *p = line + 3;
  while (*p == ' ')
    p++;
  const char *then = strstr(p, "; then");
  if (!then)
    then = strstr(p, " then");
  size_t len = then ? (size_t)(then - p) : strlen(p);
  while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t'))
    len--;
  if (len >= condsz)
    len = condsz - 1;
  memcpy(cond, p, len);
  cond[len] = '\0';
}

static int line_has_inline_then(const char *line) {
  return strstr(line, "; then") != NULL || strstr(line, " then") != NULL;
}

static int execute_if_block(char *lines[], int nlines, int start) {
  char cond[512];
  parse_if_condition(lines[start], cond, sizeof(cond));
  int cond_ok = run_command_line(cond) == 0;

  int i = start + 1;
  if (!line_has_inline_then(lines[start]) && i < nlines && strcmp(lines[i], "then") == 0)
    i++;

  int then_start = i;
  int then_end = nlines;
  int else_start = -1;

  while (i < nlines && strcmp(lines[i], "fi") != 0) {
    if (strcmp(lines[i], "else") == 0) {
      then_end = i;
      else_start = i + 1;
    }
    i++;
  }
  int else_end = i;
  if (else_start < 0)
    then_end = else_end;

  if (cond_ok) {
    for (int j = then_start; j < then_end; j++) {
      if (strcmp(lines[j], "then") != 0)
        run_command_line(lines[j]);
    }
  } else if (else_start >= 0) {
    for (int j = else_start; j < else_end; j++)
      run_command_line(lines[j]);
  }

  return (i < nlines) ? i + 1 : nlines;
}

static int load_script_lines(int fd, char *buf, char *lines[]) {
  size_t total = 0;
  ssize_t n;

  while (total < SCRIPT_BUF_SIZE - 1 &&
         (n = read(fd, buf + total, SCRIPT_BUF_SIZE - 1 - total)) > 0)
    total += (size_t)n;
  buf[total] = '\0';

  int count = 0;
  char *p = buf;
  while (*p && count < MAX_SCRIPT_LINES) {
    while (*p == ' ' || *p == '\t' || *p == '\r')
      p++;
    if (*p == '\0')
      break;
    if (*p == '#') {
      while (*p && *p != '\n')
        p++;
      if (*p == '\n')
        p++;
      continue;
    }
    lines[count++] = p;
    while (*p && *p != '\n')
      p++;
    if (*p == '\n') {
      *p = '\0';
      p++;
    }
    trim_line(lines[count - 1]);
    if (lines[count - 1][0] == '\0')
      count--;
  }
  return count;
}

static void execute_script_lines(char *lines[], int nlines) {
  for (int i = 0; i < nlines;) {
    if (strncmp(lines[i], "if ", 3) == 0) {
      i = execute_if_block(lines, nlines, i);
    } else if (strcmp(lines[i], "then") == 0 || strcmp(lines[i], "else") == 0 ||
               strcmp(lines[i], "fi") == 0) {
      printf("sh: syntax error near '%s'\n", lines[i]);
      i++;
    } else {
      run_command_line(lines[i]);
      i++;
    }
  }
}

/* Check if a file exists and is accessible */
static int file_exists(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    close(fd);
    return 1;
  }
  return 0;
}

/* Resolve command to full path, returns 1 on success, 0 on failure */
static int resolve_command(const char *command, char *resolved_path, size_t path_size) {
  if (strchr(command, '/') != NULL) {
    snprintf(resolved_path, path_size, "%s", command);
    return file_exists(resolved_path);
  }

  const char *path_dirs[] = {"/bin", "/usr/bin", NULL};
  for (int i = 0; path_dirs[i] != NULL; i++) {
    snprintf(resolved_path, path_size, "%s/%s", path_dirs[i], command);
    if (file_exists(resolved_path)) {
      return 1;
    }
  }
  return 0;
}

/* Expand * to list all files in current directory */
static void expand_glob(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0;
  size_t out_pos = 0;

  while (input[in_pos] != '\0' && out_pos < output_size - 1) {
    if (input[in_pos] == '*') {
      int fd = open(".", O_RDONLY);
      if (fd >= 0) {
        struct dirent entries[64];
        int n = getdents(fd, entries, sizeof(entries));
        if (n > 0) {
          int num_entries = n / sizeof(struct dirent);
          bool first = true;

          for (int i = 0; i < num_entries; i++) {
            if (strcmp(entries[i].d_name, ".") == 0 || strcmp(entries[i].d_name, "..") == 0)
              continue;

            if (!first && out_pos < output_size - 1)
              output[out_pos++] = ' ';
            first = false;

            const char *name = entries[i].d_name;
            size_t name_len = strlen(name);
            for (size_t j = 0; j < name_len && out_pos < output_size - 1; j++)
              output[out_pos++] = name[j];
          }
        }
        close(fd);
      }
      in_pos++;
    } else {
      output[out_pos++] = input[in_pos++];
    }
  }
  output[out_pos] = '\0';
}

/* Expand $0, $1, $2, etc. in the input string with script arguments */
static void expand_args(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0;
  size_t out_pos = 0;

  while (input[in_pos] != '\0' && out_pos < output_size - 1) {
    if (input[in_pos] == '$' && input[in_pos + 1] >= '0' && input[in_pos + 1] <= '9') {
      int arg_num = input[in_pos + 1] - '0';
      in_pos += 2;

      if (arg_num < script_argc && script_argv[arg_num] != NULL) {
        const char *arg = script_argv[arg_num];
        size_t arg_len = strlen(arg);
        for (size_t i = 0; i < arg_len && out_pos < output_size - 1; i++)
          output[out_pos++] = arg[i];
      }
    } else {
      output[out_pos++] = input[in_pos++];
    }
  }
  output[out_pos] = '\0';
}

static void echo_write_arg(int fd, const char *s) {
  write(fd, s, strlen(s));
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

int parse_and_exec(char *buf) {
  char command_buf[COMMAND_BUF_SIZE];
  char *argv[16];
  int argc = 0;

  size_t i = 0;
  size_t cmd_len = 0;

  while (i < COMMAND_BUF_SIZE - 1 && buf[i] != ' ' && buf[i] != '\0')
    command_buf[cmd_len++] = buf[i++];
  command_buf[cmd_len] = '\0';

  if (cmd_len == 0)
    return 0;

  char full_path[256];
  argv[argc++] = full_path;

  while (buf[i] != '\0' && argc < 15) {
    while (buf[i] == ' ') i++;
    if (buf[i] == '\0') break;
    if (buf[i] == '"') {
      /* Quoted token: strip the surrounding quotes, treat contents as one arg. */
      i++; /* skip opening quote */
      argv[argc++] = (char *)&buf[i];
      while (buf[i] != '"' && buf[i] != '\0') i++;
      if (buf[i] == '"') { ((char *)buf)[i] = '\0'; i++; }
    } else {
      argv[argc++] = (char *)&buf[i];
      while (buf[i] != ' ' && buf[i] != '\0') i++;
      if (buf[i] == ' ') { ((char *)buf)[i] = '\0'; i++; }
    }
  }
  argv[argc] = NULL;

  /* Scan for redirections: pull them out of argv */
  const char *redirect_out = NULL;
  const char *redirect_in = NULL;
  int append_mode = 0;

  for (int ri = 1; ri < argc - 1; ri++) {
    if (argv[ri][0] == '>' && argv[ri][1] == '>') {
      redirect_out = argv[ri + 1];
      append_mode = 1;
      for (int rj = ri; rj < argc - 2; rj++)
        argv[rj] = argv[rj + 2];
      argc -= 2;
      argv[argc] = NULL;
      ri--;
    } else if (argv[ri][0] == '>' && argv[ri][1] == '\0') {
      redirect_out = argv[ri + 1];
      append_mode = 0;
      for (int rj = ri; rj < argc - 2; rj++)
        argv[rj] = argv[rj + 2];
      argc -= 2;
      argv[argc] = NULL;
      ri--;
    } else if (argv[ri][0] == '<' && argv[ri][1] == '\0') {
      redirect_in = argv[ri + 1];
      for (int rj = ri; rj < argc - 2; rj++)
        argv[rj] = argv[rj + 2];
      argc -= 2;
      argv[argc] = NULL;
      ri--;
    }
  }

  /* Builtins */
  if (strcmp("echo", command_buf) == 0) {
    int out_fd = 1;
    if (redirect_out != NULL) {
      int flags = O_WRONLY | O_CREAT;
      flags |= append_mode ? O_APPEND : O_TRUNC;
      out_fd = open(redirect_out, flags);
      if (out_fd < 0) {
        printf("sh: cannot open '%s'\n", redirect_out);
        return 1;
      }
    }
    builtin_echo(argc, argv, out_fd);
    if (redirect_out != NULL)
      close(out_fd);
    return 0;
  }

  if (strcmp("[", command_buf) == 0 || strcmp("test", command_buf) == 0)
    return builtin_test(argc, argv);

  if (strcmp("exec", command_buf) == 0) {
    if (argc < 2) {
      printf("exec: usage: exec command [args...]\n");
      return 1;
    }

    if (!resolve_command(argv[1], full_path, sizeof(full_path))) {
      printf("sh: exec: %s: command not found\n", argv[1]);
      return 127;
    }

    char *new_argv[16];
    int new_argc = 0;
    new_argv[new_argc++] = full_path;
    for (int j = 2; j < argc && new_argc < 15; j++)
      new_argv[new_argc++] = argv[j];
    new_argv[new_argc] = NULL;

    if (redirect_in != NULL) {
      int fd = open(redirect_in, O_RDONLY);
      if (fd >= 0) { dup2(fd, 0); close(fd); }
    }
    if (redirect_out != NULL) {
      int flags = O_WRONLY | O_CREAT;
      flags |= append_mode ? O_APPEND : O_TRUNC;
      int fd = open(redirect_out, flags);
      if (fd >= 0) { dup2(fd, 1); close(fd); }
    }

    char *envp[] = { NULL };
    execve(full_path, new_argv, envp);
    printf("sh: exec: %s failed\n", argv[1]);
    return 1;
  }

  if (strcmp("cd", command_buf) == 0)
    return builtin_cd(argc, argv);
  if (strcmp("pwd", command_buf) == 0)
    return builtin_pwd();
  if (strcmp("history", command_buf) == 0)
    return builtin_history();
  if (strcmp("exit", command_buf) == 0) {
    int code = 0;
    if (argc > 1)
      code = atoi(argv[1]);
    exit(code);
  }

  /* External command — switch to canonical so the child gets a normal TTY. */
  ioctl(0, TCSCANON, (void *)0);

  if (!resolve_command(command_buf, full_path, sizeof(full_path))) {
    printf("sh: command not found: %s\n", command_buf);
    ioctl(0, TCSRAW, (void *)0);
    return 127;
  }

  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);

    if (redirect_in != NULL) {
      int fd = open(redirect_in, O_RDONLY);
      if (fd < 0) { printf("sh: cannot open '%s' for reading\n", redirect_in); exit(1); }
      dup2(fd, 0);
      close(fd);
    }

    if (redirect_out != NULL) {
      int flags = O_WRONLY | O_CREAT;
      flags |= append_mode ? O_APPEND : O_TRUNC;
      int fd = open(redirect_out, flags);
      if (fd < 0) { printf("sh: cannot open '%s' for writing\n", redirect_out); exit(1); }
      dup2(fd, 1);
      close(fd);
    }

    char *envp[] = { NULL };
    int ret = execve(full_path, argv, envp);
    /* execve only returns on failure */
    if (ret == -EACCES)
      printf("sh: %s: Permission denied\n", full_path);
    else if (ret == -ENOENT)
      printf("sh: %s: No such file or directory\n", full_path);
    else if (ret == -EISDIR)
      printf("sh: %s: Is a directory\n", full_path);
    else if (ret == -ENOEXEC)
      printf("sh: %s: Exec format error\n", full_path);
    else
      printf("sh: %s: Cannot execute (error %d)\n", full_path, ret);
    exit(1);
  } else if (pid > 0) {
    setpgid(pid, pid);
    tcsetpgrp(0, pid);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    /* Clean up any leftover members of the job's process group (e.g.
     * children that called setpgid into a sub-group of the job). */
    kill(-pid, SIGHUP);
    tcsetpgrp(0, getpid());
    ioctl(0, TCSRAW, (void *)0);
    return WEXITSTATUS(status);
  }

  printf("sh: fork failed\n");
  ioctl(0, TCSRAW, (void *)0);
  return 1;
}

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int main(int argc, char **argv, char **envp) {
  // Handle -c option: sh -c "command string"
  if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
    script_argv = argv;
    script_argc = argc;

    char line[1024];
    strncpy(line, argv[2], sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    return run_command_line(line);
  }

  // If a script file is provided as argv[1], execute it and exit
  if (argc >= 2) {
    const char *script_path = argv[1];

    size_t path_len = strlen(script_path);
    char stripped_path[256];
    if (path_len >= 2 && script_path[0] == '"' && script_path[path_len - 1] == '"') {
      for (size_t j = 1; j < path_len - 1 && j < sizeof(stripped_path); j++)
        stripped_path[j - 1] = script_path[j];
      stripped_path[path_len - 2] = '\0';
      script_path = stripped_path;
    }

    script_argv = argv;
    script_argc = argc;

    int fd = open(script_path, O_RDONLY);
    if (fd < 0) {
      printf("sh: cannot open script '%s'\n", script_path);
      return 1;
    }

    static char script_buf[SCRIPT_BUF_SIZE];
    static char *script_lines[MAX_SCRIPT_LINES];
    int nlines = load_script_lines(fd, script_buf, script_lines);
    close(fd);
    execute_script_lines(script_lines, nlines);
    return 0;
  }

  // Interactive mode
  (void)envp;
  setpgid(0, 0);               /* become our own process group leader */
  pid_t shell_pgid = getpid();
  tcsetpgrp(0, shell_pgid);

  signal(SIGINT,  SIG_IGN);   /* shell ignores Ctrl-C at prompt */
  signal(SIGTSTP, SIG_IGN);   /* shell ignores Ctrl-Z at prompt */

  history_load();
  ioctl(0, TCSRAW, (void*)0);

  char buf[COMMAND_BUF_SIZE];
  char cwd[256];
  char prompt[300];

  while (1) {
    if (getcwd(cwd, sizeof(cwd)) == NULL)
      cwd[0] = '\0';

    int pi = 0;
    for (int j = 0; cwd[j] && pi < 280; j++) prompt[pi++] = cwd[j];
    prompt[pi++] = ' '; prompt[pi++] = '$'; prompt[pi++] = ' ';
    prompt[pi] = '\0';

    int n = readline_with_history(prompt, buf, sizeof(buf));
    if (n > 0) {
      history_push(buf);
      run_command_line(buf);
    }
  }

  return 0;
}
