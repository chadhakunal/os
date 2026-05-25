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
#include <signal.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define COMMAND_BUF_SIZE  256
#define MAX_HISTORY       128
#define HISTORY_FILE      "/.history"
#define SCRIPT_BUF_SIZE   65536
#define MAX_SCRIPT_LINES  2048
#define MAX_JOBS          16

/* -------------------------------------------------------------------------
 * Job table
 * ---------------------------------------------------------------------- */
struct job {
  pid_t  pid;
  int    id;
  int    stopped; /* 1 = Ctrl-Z stopped, 0 = running background */
  char   cmd[COMMAND_BUF_SIZE];
};

static struct job jobs[MAX_JOBS];
static int        njobs = 0;
static int        next_job_id = 1;

static void job_add(pid_t pid, const char *cmd, int stopped) {
  if (njobs >= MAX_JOBS) return;
  jobs[njobs].pid     = pid;
  jobs[njobs].id      = next_job_id++;
  jobs[njobs].stopped = stopped;
  strncpy(jobs[njobs].cmd, cmd, COMMAND_BUF_SIZE - 1);
  jobs[njobs].cmd[COMMAND_BUF_SIZE - 1] = '\0';
  njobs++;
}

static void job_remove(pid_t pid) {
  for (int i = 0; i < njobs; i++) {
    if (jobs[i].pid == pid) {
      jobs[i] = jobs[--njobs];
      return;
    }
  }
}

/* Reap finished background jobs, printing Done notices to stderr. */
static void jobs_reap(void) {
  for (int i = 0; i < njobs; i++) {
    if (jobs[i].stopped) continue;
    int st = 0;
    pid_t r = waitpid(jobs[i].pid, &st, WNOHANG);
    if (r > 0) {
      fprintf(stderr, "[%d]+ Done    %s\n", jobs[i].id, jobs[i].cmd);
      jobs[i] = jobs[--njobs];
      i--;
    }
  }
}

static struct job *job_find_last_stopped(void) {
  for (int i = njobs - 1; i >= 0; i--)
    if (jobs[i].stopped) return &jobs[i];
  return NULL;
}

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
  int fd = open(HISTORY_FILE, O_WRONLY | O_CREAT, 0666);
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

/* Move terminal cursor left/right by n columns. */
static void term_move(int n, int left) {
  char seq[16];
  int len = 0;
  seq[len++] = '\x1b'; seq[len++] = '[';
  if (n >= 10) seq[len++] = '0' + n / 10;
  seq[len++] = '0' + n % 10;
  seq[len++] = left ? 'D' : 'C';
  write(1, seq, len);
}

/* Redraw from cursor position to end of line, then reposition cursor. */
static void term_redraw_tail(const char *line, int pos, int len) {
  /* write chars from pos to end */
  write(1, line + pos, len - pos);
  /* write spaces to erase any leftover chars (handles delete-in-middle) */
  write(1, " ", 1);
  /* move cursor back to pos */
  term_move(len - pos + 1, 1);
}

static int readline_with_history(const char *prompt, char *out_buf,
                                  int max_len) {
  char line[COMMAND_BUF_SIZE];
  int  line_len = 0;
  int  pos = 0; /* cursor position within line */

  char saved_line[COMMAND_BUF_SIZE] = {0};
  int  hist_pos = history_count;

  write(1, prompt, strlen(prompt));

  while (1) {
    char c;
    if (read(0, &c, 1) <= 0) {
      continue;
    }

    /* Ctrl-C — clear line, reshow prompt */
    if (c == 0x03) {
      if (pos < line_len) term_move(line_len - pos, 0);
      for (int i = 0; i < line_len; i++) write(1, "\b \b", 3);
      write(1, "^C\n", 3);
      write(1, prompt, strlen(prompt));
      line_len = 0; pos = 0;
      continue;
    }

    /* Ctrl-Z — ignore silently (SIGTSTP already sent to foreground PGID by TTY) */
    if (c == 0x1a) {
      continue;
    }

    if (c == '\n' || c == '\r') {
      /* Move to end before submitting so output starts on a fresh line. */
      if (pos < line_len) write(1, line + pos, line_len - pos);
      write(1, "\n", 1);
      line[line_len] = '\0';
      memcpy(out_buf, line, line_len + 1);
      return line_len;
    }

    /* Backspace / DEL — delete char before cursor. */
    if (c == 0x7f || c == 0x08) {
      if (pos > 0) {
        memmove(line + pos - 1, line + pos, line_len - pos);
        pos--; line_len--;
        write(1, "\b", 1);
        term_redraw_tail(line, pos, line_len);
      }
      continue;
    }

    /* Ctrl-L — clear screen, redraw prompt and current input. */
    if (c == 0x0c) {
      write(1, "\x1b[2J\x1b[H", 8);
      write(1, prompt, strlen(prompt));
      write(1, line, line_len);
      if (pos < line_len) term_move(line_len - pos, 1);
      continue;
    }

    if (c == '\x1b') {
      char seq[3];
      if (read(0, &seq[0], 1) <= 0) continue;
      if (seq[0] != '[') continue;
      if (read(0, &seq[1], 1) <= 0) continue;

      if (seq[1] == 'A') {
        /* Up — older history. */
        if (hist_pos == 0) continue;
        if (hist_pos == history_count)
          memcpy(saved_line, line, line_len + 1);
        hist_pos--;
        /* erase current line */
        if (pos < line_len) write(1, line + pos, line_len - pos);
        for (int i = 0; i < line_len; i++) write(1, "\b \b", 3);
        line_len = strlen(history[hist_pos]);
        memcpy(line, history[hist_pos], line_len + 1);
        pos = line_len;
        write(1, line, line_len);
      } else if (seq[1] == 'B') {
        /* Down — newer entry or restore saved input. */
        if (hist_pos == history_count) continue;
        hist_pos++;
        if (pos < line_len) write(1, line + pos, line_len - pos);
        for (int i = 0; i < line_len; i++) write(1, "\b \b", 3);
        if (hist_pos == history_count) {
          line_len = strlen(saved_line);
          memcpy(line, saved_line, line_len + 1);
        } else {
          line_len = strlen(history[hist_pos]);
          memcpy(line, history[hist_pos], line_len + 1);
        }
        pos = line_len;
        write(1, line, line_len);
      } else if (seq[1] == 'C') {
        /* Right arrow — move cursor right. */
        if (pos < line_len) {
          write(1, &line[pos], 1);
          pos++;
        }
      } else if (seq[1] == 'D') {
        /* Left arrow — move cursor left. */
        if (pos > 0) {
          pos--;
          write(1, "\b", 1);
        }
      }
      continue;
    }

    /* Printable character — insert at cursor position. */
    if (c >= 0x20 && line_len < max_len - 1) {
      memmove(line + pos + 1, line + pos, line_len - pos);
      line[pos] = c; pos++; line_len++;
      write(1, &c, 1);
      if (pos < line_len)
        term_redraw_tail(line, pos, line_len);
    }
  }
}

/* -------------------------------------------------------------------------
 * Builtins
 * ---------------------------------------------------------------------- */

// Global variables for script arguments
static char **script_argv = NULL;
static int script_argc = 0;

// Last command exit status — exposed as $?
static int last_exit_status = 0;
// PID of last background job — exposed as $!
static pid_t last_bg_pid = 0;

/* Convert a waitpid status word to a shell exit code.
 * Signal-killed: 128 + signo. Normal exit: exit code. */
static int wait_status_to_exit(int st) {
  if (WIFSIGNALED(st))
    return 128 + WTERMSIG(st);
  return WEXITSTATUS(st);
}

/* -------------------------------------------------------------------------
 * Shell variable store (NAME=value, not exported to env unless export used)
 * ---------------------------------------------------------------------- */
#define MAX_SHELL_VARS 256
#define MAX_VAR_NAME   64
#define MAX_VAR_VALUE  1024

static struct {
  char name[MAX_VAR_NAME];
  char value[MAX_VAR_VALUE];
} shell_vars[MAX_SHELL_VARS];
static int shell_var_count = 0;

static const char *shell_var_get(const char *name) {
  for (int i = 0; i < shell_var_count; i++)
    if (strcmp(shell_vars[i].name, name) == 0)
      return shell_vars[i].value;
  return NULL;
}

static void shell_var_set(const char *name, const char *value) {
  for (int i = 0; i < shell_var_count; i++) {
    if (strcmp(shell_vars[i].name, name) == 0) {
      strncpy(shell_vars[i].value, value, MAX_VAR_VALUE - 1);
      shell_vars[i].value[MAX_VAR_VALUE - 1] = '\0';
      return;
    }
  }
  if (shell_var_count < MAX_SHELL_VARS) {
    strncpy(shell_vars[shell_var_count].name,  name,  MAX_VAR_NAME  - 1);
    strncpy(shell_vars[shell_var_count].value, value, MAX_VAR_VALUE - 1);
    shell_vars[shell_var_count].name[MAX_VAR_NAME   - 1] = '\0';
    shell_vars[shell_var_count].value[MAX_VAR_VALUE - 1] = '\0';
    shell_var_count++;
  }
}

int parse_and_exec(char *buf);
static int run_command_line(char *line);
static void execute_script_lines(char *lines[], int nlines);

static void expand_args(const char *input, char *output, size_t output_size);
static void expand_glob(const char *input, char *output, size_t output_size);
static void expand_backticks(const char *input, char *output, size_t output_size);

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

  /* Unary file tests */
  if (i + 1 < argc) {
    const char *op  = argv[i];
    const char *arg = argv[i + 1];
    if (strcmp(op, "-x") == 0) {
      struct stat st;
      if (stat(arg, &st) != 0) return negate ? 0 : 1;
      int ok = (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-r") == 0) {
      struct stat st;
      if (stat(arg, &st) != 0) return negate ? 0 : 1;
      int ok = (st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) != 0;
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-w") == 0) {
      struct stat st;
      if (stat(arg, &st) != 0) return negate ? 0 : 1;
      int ok = (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) != 0;
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-s") == 0) {
      struct stat st;
      int ok = stat(arg, &st) == 0 && st.st_size > 0;
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-f") == 0) {
      struct stat st;
      int ok = stat(arg, &st) == 0 && S_ISREG(st.st_mode);
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-d") == 0) {
      struct stat st;
      int ok = stat(arg, &st) == 0 && S_ISDIR(st.st_mode);
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-e") == 0) {
      struct stat st;
      int ok = stat(arg, &st) == 0;
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-z") == 0) {
      int ok = (arg[0] == '\0');
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
    if (strcmp(op, "-n") == 0) {
      int ok = (arg[0] != '\0');
      return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
    }
  }

  /* Binary comparisons: str op str */
  if (i + 2 < argc) {
    const char *lhs = argv[i];
    const char *op  = argv[i + 1];
    const char *rhs = argv[i + 2];
    int ok = 0;
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0)
      ok = strcmp(lhs, rhs) == 0;
    else if (strcmp(op, "!=") == 0)
      ok = strcmp(lhs, rhs) != 0;
    else {
      int l = atoi(lhs), r = atoi(rhs);
      if      (strcmp(op, "-eq") == 0) ok = l == r;
      else if (strcmp(op, "-ne") == 0) ok = l != r;
      else if (strcmp(op, "-lt") == 0) ok = l <  r;
      else if (strcmp(op, "-le") == 0) ok = l <= r;
      else if (strcmp(op, "-gt") == 0) ok = l >  r;
      else if (strcmp(op, "-ge") == 0) ok = l >= r;
    }
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  /* Single argument: true if non-empty string */
  if (i < argc) {
    int ok = (argv[i][0] != '\0');
    return negate ? (ok ? 1 : 0) : (ok ? 0 : 1);
  }

  return negate ? 0 : 1;
}

/* Execute a pipeline: one or more commands separated by '|'.
 * Each command runs in a child; stdout of command[i] feeds stdin of command[i+1].
 * The last command's exit status is returned. */
static int run_pipeline(char *segs[], int nseg) {
  if (nseg == 1)
    return parse_and_exec(segs[0]);

  int prev_read = -1;
  pid_t pids[16];

  for (int i = 0; i < nseg; i++) {
    int fds[2] = {-1, -1};
    if (i < nseg - 1) {
      if (pipe(fds) < 0) { perror("pipe"); return 1; }
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
      /* child: wire up stdin from previous pipe */
      if (prev_read != -1) { dup2(prev_read, 0); close(prev_read); }
      /* wire up stdout to write end of next pipe */
      if (fds[1] != -1) { dup2(fds[1], 1); close(fds[1]); }
      if (fds[0] != -1) close(fds[0]);
      /* switch TTY to canonical for child */
      ioctl(0, TCSCANON, (void *)0);
      exit(parse_and_exec(segs[i]));
    }

    pids[i] = pid;
    if (prev_read != -1) close(prev_read);
    if (fds[1]  != -1) close(fds[1]);
    prev_read = fds[0];
  }

  /* wait for all children; return last exit status */
  int status = 0;
  for (int i = 0; i < nseg; i++) {
    int st = 0;
    waitpid(pids[i], &st, 0);
    if (i == nseg - 1) status = WEXITSTATUS(st);
  }
  return status;
}

static int run_command_line(char *line) {
  int last = 0;

  trim_line(line);
  if (line[0] == '\0' || line[0] == '#')
    return 0;

  /* Inline compound commands: if/while/for on a single line with ; then/do.
   * Route directly to execute_script_lines so the semicolons inside are
   * not mistaken for statement separators. */
  if ((strncmp(line, "if ",    3) == 0 && (strstr(line, "; then") || strstr(line, " then "))) ||
      (strncmp(line, "while ", 6) == 0 && (strstr(line, "; do")   || strstr(line, " do ")))   ||
      (strncmp(line, "for ",   4) == 0 && (strstr(line, "; do")   || strstr(line, " do ")))) {
    static char *sl[1];
    sl[0] = line;
    execute_script_lines(sl, 1);
    return last_exit_status;
  }

  /* Split the raw line on ; && || into statements BEFORE variable expansion,
   * so that $? in later statements sees the exit status of earlier ones. */
  static char stmts[32][1024];
  static int  sep[32]; /* separator preceding stmt[i]: 0=; 1=&& 2=|| */
  int nstmts = 0;

  char *p = line;
  while (*p && nstmts < 32) {
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') break;
    char *start = p;
    int in_q = 0;
    char q_char = 0;
    int next_sep = 0;
    while (*p) {
      if (!in_q && (*p == '"' || *p == '\'')) { in_q = 1; q_char = *p; p++; continue; }
      if (in_q && *p == q_char) { in_q = 0; q_char = 0; p++; continue; }
      if (!in_q) {
        if (*p == ';') { next_sep = 0; break; }
        if (*p == '&' && *(p+1) == '&') { next_sep = 1; break; }
        if (*p == '|' && *(p+1) == '|') { next_sep = 2; break; }
        /* bare & — end of a background command; treat like ; */
        if (*p == '&' && *(p+1) != '&') { next_sep = 3; break; }
      }
      p++;
    }
    size_t len = (size_t)(p - start);
    /* advance past the separator */
    if (*p == ';') p++;
    else if ((*p == '&' || *p == '|') && *(p+1) == *p) p += 2;
    else if (*p == '&') p++; /* bare & */
    if (len >= 1024) len = 1023;
    memcpy(stmts[nstmts], start, len);
    stmts[nstmts][len] = '\0';
    trim_line(stmts[nstmts]);
    /* bare & separator: append & so parse_and_exec backgrounds this command */
    if (next_sep == 3) {
      size_t slen = strlen(stmts[nstmts]);
      if (slen + 3 < 1024) {
        stmts[nstmts][slen]   = ' ';
        stmts[nstmts][slen+1] = '&';
        stmts[nstmts][slen+2] = '\0';
      }
    }
    if (stmts[nstmts][0] != '\0') {
      sep[nstmts] = next_sep;
      nstmts++;
    }
  }

  for (int si = 0; si < nstmts; si++) {
    char *stmt = stmts[si];

    /* honour && and || from previous statement's separator */
    if (si > 0) {
      int prev_sep = sep[si - 1];
      if (prev_sep == 1 && last != 0) break; /* && : stop on failure */
      if (prev_sep == 2 && last == 0) break; /* || : stop on success */
    }

    /* Expand variables/backticks/globs per-statement so $? reflects the
     * exit status of the preceding statement in this line. */
    static char bt_expanded[1024];
    static char var_expanded[1024];
    static char gl_expanded[1024];
    expand_backticks(stmt, bt_expanded, sizeof(bt_expanded));
    expand_args(bt_expanded, var_expanded, sizeof(var_expanded));
    expand_glob(var_expanded, gl_expanded, sizeof(gl_expanded));

    /* split on '|' (not inside quotes) for pipeline */
    char *segs[16];
    int nseg = 0;
    char *q = gl_expanded;
    segs[nseg++] = q;
    int in_quote = 0;
    char q_char2 = 0;
    for (; *q; q++) {
      if (!in_quote && (*q == '"' || *q == '\'')) { in_quote = 1; q_char2 = *q; continue; }
      if (in_quote && *q == q_char2) { in_quote = 0; q_char2 = 0; continue; }
      if (!in_quote && *q == '|' && *(q+1) != '|') {
        *q = '\0';
        trim_line(segs[nseg - 1]);
        if (nseg < 15) segs[nseg++] = q + 1;
      }
    }
    trim_line(segs[nseg - 1]);

    if (nseg > 1)
      ioctl(0, TCSCANON, (void *)0);

    last = run_pipeline(segs, nseg);
    last_exit_status = last;

    if (nseg > 1)
      ioctl(0, TCSRAW, (void *)0);
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

  /* Fully inline: if cond; then body; else body; fi — all on one line */
  const char *line = lines[start];
  const char *then_kw = strstr(line, "; then");
  if (!then_kw) then_kw = strstr(line, " then ");
  if (then_kw) {
    /* Find the body after "then" */
    const char *body = then_kw + (line[then_kw - line] == ';' ? 6 : 6);
    /* Skip past "; then " */
    while (*body == ' ') body++;

    /* Find optional "; else" and "; fi" */
    const char *else_kw = strstr(body, "; else");
    const char *fi_kw   = strstr(body, "; fi");

    if (else_kw || fi_kw) {
      /* It's a fully inline if statement */
      char then_body[512] = {0};
      char else_body[512] = {0};

      if (else_kw) {
        size_t tlen = (size_t)(else_kw - body);
        if (tlen >= sizeof(then_body)) tlen = sizeof(then_body) - 1;
        memcpy(then_body, body, tlen);
        then_body[tlen] = '\0';
        const char *eb = else_kw + 6; /* skip "; else" */
        while (*eb == ' ') eb++;
        /* strip trailing "; fi" */
        const char *efi = strstr(eb, "; fi");
        size_t elen = efi ? (size_t)(efi - eb) : strlen(eb);
        if (elen >= sizeof(else_body)) elen = sizeof(else_body) - 1;
        memcpy(else_body, eb, elen);
        else_body[elen] = '\0';
      } else {
        /* no else — body ends at "; fi" */
        size_t tlen = (size_t)(fi_kw - body);
        if (tlen >= sizeof(then_body)) tlen = sizeof(then_body) - 1;
        memcpy(then_body, body, tlen);
        then_body[tlen] = '\0';
      }

      if (cond_ok)
        run_command_line(then_body);
      else if (else_body[0])
        run_command_line(else_body);

      return start + 1;
    }
  }

  /* Multi-line form */
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
        execute_script_lines(lines + j, 1);
    }
  } else if (else_start >= 0) {
    for (int j = else_start; j < else_end; j++)
      execute_script_lines(lines + j, 1);
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

/* Execute a while loop block starting at lines[start] ("while <cond>").
 * Returns the index of the line after "done". */
static int execute_while_block(char *lines[], int nlines, int start) {
  /* extract condition — everything after "while " */
  char cond[512];
  strncpy(cond, lines[start] + 6, sizeof(cond) - 1);
  cond[sizeof(cond) - 1] = '\0';
  trim_line(cond);

  /* skip optional "do" line */
  int body_start = start + 1;
  if (body_start < nlines && strcmp(lines[body_start], "do") == 0)
    body_start++;

  /* find matching "done" */
  int body_end = body_start;
  while (body_end < nlines && strcmp(lines[body_end], "done") != 0)
    body_end++;

  /* run while condition holds */
  while (run_command_line(cond) == 0)
    execute_script_lines(lines + body_start, body_end - body_start);

  return body_end < nlines ? body_end + 1 : nlines;
}

/* Execute a for loop block: "for VAR in word...".
 * Returns the index of the line after "done". */
static int execute_for_block(char *lines[], int nlines, int start) {
  /* parse: "for VAR in w1 w2 ..." */
  char header[512];
  strncpy(header, lines[start] + 4, sizeof(header) - 1);
  header[sizeof(header) - 1] = '\0';
  trim_line(header);

  char var[64] = {0};
  char *p = header;
  size_t vlen = 0;
  while (*p && *p != ' ' && vlen < sizeof(var) - 1) var[vlen++] = *p++;
  var[vlen] = '\0';

  /* skip " in " */
  while (*p == ' ') p++;
  if (strncmp(p, "in", 2) == 0) p += 2;
  while (*p == ' ') p++;

  /* collect words — expand variables and globs */
  char words_raw[512];
  strncpy(words_raw, p, sizeof(words_raw) - 1);
  words_raw[sizeof(words_raw) - 1] = '\0';
  char words_exp[512];
  expand_args(words_raw, words_exp, sizeof(words_exp));
  char words[512];
  expand_glob(words_exp, words, sizeof(words));

  /* skip optional "do" */
  int body_start = start + 1;
  if (body_start < nlines && strcmp(lines[body_start], "do") == 0)
    body_start++;

  /* find matching "done" */
  int body_end = body_start;
  while (body_end < nlines && strcmp(lines[body_end], "done") != 0)
    body_end++;

  /* iterate over words */
  char *w = words;
  while (*w) {
    while (*w == ' ') w++;
    if (!*w) break;
    char word[256];
    size_t wlen = 0;
    while (*w && *w != ' ' && wlen < sizeof(word) - 1) word[wlen++] = *w++;
    word[wlen] = '\0';
    shell_var_set(var, word);
    execute_script_lines(lines + body_start, body_end - body_start);
  }

  return body_end < nlines ? body_end + 1 : nlines;
}

static void execute_script_lines(char *lines[], int nlines) {
  for (int i = 0; i < nlines;) {
    if (strncmp(lines[i], "if ", 3) == 0) {
      i = execute_if_block(lines, nlines, i);
    } else if (strncmp(lines[i], "while ", 6) == 0) {
      i = execute_while_block(lines, nlines, i);
    } else if (strncmp(lines[i], "for ", 4) == 0) {
      i = execute_for_block(lines, nlines, i);
    } else if (strcmp(lines[i], "then") == 0 || strcmp(lines[i], "else") == 0 ||
               strcmp(lines[i], "fi") == 0   || strcmp(lines[i], "do") == 0 ||
               strcmp(lines[i], "done") == 0) {
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

  const char *path_env = getenv("PATH");
  if (!path_env || !*path_env)
    path_env = "/bin";

  char path_buf[1024];
  strncpy(path_buf, path_env, sizeof(path_buf) - 1);
  path_buf[sizeof(path_buf) - 1] = '\0';

  char *dir = path_buf;
  while (dir && *dir) {
    char *colon = strchr(dir, ':');
    if (colon) *colon = '\0';
    snprintf(resolved_path, path_size, "%s/%s", dir, command);
    if (file_exists(resolved_path))
      return 1;
    dir = colon ? colon + 1 : NULL;
  }
  return 0;
}

/* Expand * to list all files in current directory */
static void expand_glob(const char *input, char *output, size_t output_size) {
  /* Process input token by token. Only tokens containing '*' are globbed. */
  size_t out_pos = 0;
  const char *p = input;

  while (*p && out_pos < output_size - 1) {
    /* Copy leading spaces. */
    if (*p == ' ') { output[out_pos++] = *p++; continue; }

    /* Collect one whitespace-delimited token. */
    char token[512];
    size_t tlen = 0;
    while (*p && *p != ' ' && tlen < sizeof(token) - 1)
      token[tlen++] = *p++;
    token[tlen] = '\0';

    if (!strchr(token, '*')) {
      /* No glob — copy literally. */
      for (size_t k = 0; k < tlen && out_pos < output_size - 1; k++)
        output[out_pos++] = token[k];
      continue;
    }

    /* Split token at '*': find dir and name prefix/suffix. */
    char *star = strchr(token, '*');
    char dir[512] = ".";
    char prefix[256] = "";
    char suffix[256] = "";

    /* Everything before '*'. */
    char pre[512];
    size_t prelen = (size_t)(star - token);
    memcpy(pre, token, prelen);
    pre[prelen] = '\0';

    /* Split pre into dir and name-prefix at last '/'. */
    char *last_slash = strrchr(pre, '/');
    if (last_slash) {
      size_t dlen = (size_t)(last_slash - pre);
      if (dlen == 0) { dir[0] = '/'; dir[1] = '\0'; }
      else { memcpy(dir, pre, dlen); dir[dlen] = '\0'; }
      strncpy(prefix, last_slash + 1, sizeof(prefix) - 1);
    } else {
      strncpy(prefix, pre, sizeof(prefix) - 1);
    }
    strncpy(suffix, star + 1, sizeof(suffix) - 1);

    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
      for (size_t k = 0; k < tlen && out_pos < output_size - 1; k++)
        output[out_pos++] = token[k];
      continue;
    }

    struct dirent entries[64];
    int n = getdents(fd, entries, 64);
    close(fd);
    int num = n > 0 ? n : 0;
    size_t plen = strlen(prefix), sfxlen = strlen(suffix);
    bool matched = false;

    for (int i = 0; i < num; i++) {
      const char *name = entries[i].d_name;
      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
      size_t nlen = strlen(name);
      if (nlen < plen + sfxlen) continue;
      if (plen && strncmp(name, prefix, plen) != 0) continue;
      if (sfxlen && strcmp(name + nlen - sfxlen, suffix) != 0) continue;

      /* Separate matches with spaces. */
      if (matched && out_pos < output_size - 1) output[out_pos++] = ' ';
      matched = true;

      if (strcmp(dir, ".") != 0) {
        size_t dl = strlen(dir);
        for (size_t k = 0; k < dl && out_pos < output_size - 1; k++)
          output[out_pos++] = dir[k];
        if (out_pos < output_size - 1) output[out_pos++] = '/';
      }
      for (const char *q = name; *q && out_pos < output_size - 1; q++)
        output[out_pos++] = *q;
    }

    if (!matched) {
      /* No match — emit original token literally. */
      for (size_t k = 0; k < tlen && out_pos < output_size - 1; k++)
        output[out_pos++] = token[k];
    }
  }
  output[out_pos] = '\0';
}

/* Expand $0, $1, $2, etc. in the input string with script arguments */
static void expand_args(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0;
  size_t out_pos = 0;
  int in_single_quote = 0;

  while (input[in_pos] != '\0' && out_pos < output_size - 1) {
    if (input[in_pos] == '\'' && !in_single_quote) {
      /* Enter single-quote mode: pass the quote through so the tokenizer sees it. */
      in_single_quote = 1;
      output[out_pos++] = input[in_pos++];
      continue;
    }
    if (input[in_pos] == '\'' && in_single_quote) {
      in_single_quote = 0;
      output[out_pos++] = input[in_pos++];
      continue;
    }
    if (input[in_pos] != '$' || in_single_quote) {
      output[out_pos++] = input[in_pos++];
      continue;
    }

    in_pos++; /* skip '$' */

    /* $# — number of positional arguments */
    if (input[in_pos] == '#') {
      in_pos++;
      char num[12];
      snprintf(num, sizeof(num), "%d", script_argc > 0 ? script_argc - 1 : 0);
      for (size_t k = 0; num[k] && out_pos < output_size - 1; k++)
        output[out_pos++] = num[k];
      continue;
    }

    /* $@ — all positional arguments space-separated */
    if (input[in_pos] == '@') {
      in_pos++;
      for (int ai = 1; ai < script_argc; ai++) {
        if (ai > 1 && out_pos < output_size - 1) output[out_pos++] = ' ';
        const char *arg = script_argv[ai];
        for (size_t k = 0; arg[k] && out_pos < output_size - 1; k++)
          output[out_pos++] = arg[k];
      }
      continue;
    }

    /* $? — last exit status */
    if (input[in_pos] == '?') {
      in_pos++;
      char num[12];
      snprintf(num, sizeof(num), "%d", last_exit_status);
      for (size_t k = 0; num[k] && out_pos < output_size - 1; k++)
        output[out_pos++] = num[k];
      continue;
    }

    /* $! — PID of last background job */
    if (input[in_pos] == '!') {
      in_pos++;
      char num[12];
      snprintf(num, sizeof(num), "%d", (int)last_bg_pid);
      for (size_t k = 0; num[k] && out_pos < output_size - 1; k++)
        output[out_pos++] = num[k];
      continue;
    }

    /* $0-$9 — positional arguments */
    if (input[in_pos] >= '0' && input[in_pos] <= '9') {
      int arg_num = input[in_pos++] - '0';
      if (arg_num < script_argc && script_argv[arg_num] != NULL) {
        const char *arg = script_argv[arg_num];
        for (size_t k = 0; arg[k] && out_pos < output_size - 1; k++)
          output[out_pos++] = arg[k];
      }
      continue;
    }

    /* ${VAR}, ${VAR:-default}, ${VAR:+alt} — braced variable */
    if (input[in_pos] == '{') {
      in_pos++;
      char varname[MAX_VAR_NAME];
      size_t vlen = 0;
      while (input[in_pos] && input[in_pos] != '}' &&
             input[in_pos] != ':' && vlen < MAX_VAR_NAME - 1)
        varname[vlen++] = input[in_pos++];
      varname[vlen] = '\0';
      char modifier = 0;
      char defval[256];
      defval[0] = '\0';
      if (input[in_pos] == ':' && (input[in_pos+1] == '-' || input[in_pos+1] == '+')) {
        modifier = input[in_pos+1];
        in_pos += 2;
        size_t dlen = 0;
        while (input[in_pos] && input[in_pos] != '}' && dlen < sizeof(defval) - 1)
          defval[dlen++] = input[in_pos++];
        defval[dlen] = '\0';
      }
      if (input[in_pos] == '}') in_pos++;
      const char *val = shell_var_get(varname);
      if (!val) val = getenv(varname);
      const char *emit = NULL;
      if (modifier == '-')
        emit = (val && val[0]) ? val : defval;
      else if (modifier == '+')
        emit = (val && val[0]) ? defval : NULL;
      else
        emit = val;
      if (emit)
        for (size_t k = 0; emit[k] && out_pos < output_size - 1; k++)
          output[out_pos++] = emit[k];
      continue;
    }

    /* $VAR — unbraced variable: name is [A-Za-z_][A-Za-z0-9_]* */
    if ((input[in_pos] >= 'A' && input[in_pos] <= 'Z') ||
        (input[in_pos] >= 'a' && input[in_pos] <= 'z') ||
         input[in_pos] == '_') {
      char varname[MAX_VAR_NAME];
      size_t vlen = 0;
      while ((input[in_pos] >= 'A' && input[in_pos] <= 'Z') ||
             (input[in_pos] >= 'a' && input[in_pos] <= 'z') ||
             (input[in_pos] >= '0' && input[in_pos] <= '9') ||
              input[in_pos] == '_') {
        if (vlen < MAX_VAR_NAME - 1)
          varname[vlen++] = input[in_pos];
        in_pos++;
      }
      varname[vlen] = '\0';
      const char *val = shell_var_get(varname);
      if (!val) val = getenv(varname);
      if (val)
        for (size_t k = 0; val[k] && out_pos < output_size - 1; k++)
          output[out_pos++] = val[k];
      continue;
    }

    /* $(cmd) — command substitution */
    if (input[in_pos] == '(') {
      in_pos++;
      char cmd[512];
      size_t clen = 0;
      int depth = 1;
      while (input[in_pos] && depth > 0) {
        if (input[in_pos] == '(') depth++;
        else if (input[in_pos] == ')') { if (--depth == 0) { in_pos++; break; } }
        if (depth > 0 && clen < sizeof(cmd) - 1) cmd[clen++] = input[in_pos];
        in_pos++;
      }
      cmd[clen] = '\0';

      int fds[2];
      if (pipe(fds) != 0) continue;
      fflush(stdout);
      pid_t pid = fork();
      if (pid < 0) { close(fds[0]); close(fds[1]); continue; }
      if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 1);
        close(fds[1]);
        char cmd_copy[512];
        strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
        cmd_copy[sizeof(cmd_copy) - 1] = '\0';
        exit(run_command_line(cmd_copy));
      }
      close(fds[1]);
      char result[1024];
      ssize_t nr = 0;
      while (1) {
        ssize_t n = read(fds[0], result + nr, sizeof(result) - 1 - nr);
        if (n > 0) { nr += n; if ((size_t)nr >= sizeof(result) - 1) break; }
        else if (n == 0) break;
        else if (errno == EINTR) continue;
        else break;
      }
      close(fds[0]);
      int st;
      waitpid(pid, &st, 0);
      if (nr > 0) {
        while (nr > 0 && (result[nr-1] == '\n' || result[nr-1] == '\r')) nr--;
        for (ssize_t k = 0; k < nr && out_pos < output_size - 1; k++)
          output[out_pos++] = result[k];
      }
      continue;
    }

    /* bare '$' with nothing recognised after it — emit literally */
    output[out_pos++] = '$';
  }

  /* backtick command substitution `cmd` */
  /* re-scan output is complex; handle at input level instead */
  output[out_pos] = '\0';
}

/* Expand backtick substitutions in a string (called before expand_args for `...`) */
static void expand_backticks(const char *input, char *output, size_t output_size) {
  size_t in_pos = 0, out_pos = 0;
  while (input[in_pos] && out_pos < output_size - 1) {
    if (input[in_pos] != '`') {
      output[out_pos++] = input[in_pos++];
      continue;
    }
    in_pos++; /* skip opening backtick */
    char cmd[512];
    size_t clen = 0;
    while (input[in_pos] && input[in_pos] != '`' && clen < sizeof(cmd) - 1)
      cmd[clen++] = input[in_pos++];
    cmd[clen] = '\0';
    if (input[in_pos] == '`') in_pos++;

    int fds[2];
    if (pipe(fds) == 0) {
      fflush(stdout);
      pid_t pid = fork();
      if (pid == 0) {
        close(fds[0]); dup2(fds[1], 1); close(fds[1]);
        char cmd_copy[512];
        strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
        exit(run_command_line(cmd_copy));
      }
      close(fds[1]);
      char result[1024];
      ssize_t nr = 0;
      while (1) {
        ssize_t n = read(fds[0], result + nr, sizeof(result) - 1 - nr);
        if (n > 0) { nr += n; if ((size_t)nr >= sizeof(result) - 1) break; }
        else if (n == 0) break;
        else if (errno == EINTR) continue;
        else break;
      }
      close(fds[0]);
      int st; waitpid(pid, &st, 0);
      while (nr > 0 && (result[nr-1] == '\n' || result[nr-1] == '\r')) nr--;
      for (ssize_t k = 0; k < nr && out_pos < output_size - 1; k++)
        output[out_pos++] = result[k];
    }
  }
  output[out_pos] = '\0';
}

static void echo_write_arg(int fd, const char *s) {
  write(fd, s, strlen(s));
}

static int builtin_source(int argc, char *argv[]) {
  if (argc < 2) {
    printf("source: usage: source <file>\n");
    return 1;
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0) {
    printf("source: %s: cannot open file\n", argv[1]);
    return 1;
  }
  char *src_buf = malloc(SCRIPT_BUF_SIZE);
  char **src_lines = malloc(MAX_SCRIPT_LINES * sizeof(char *));
  if (!src_buf || !src_lines) {
    free(src_buf); free(src_lines);
    printf("source: out of memory\n");
    return 1;
  }
  int nlines = load_script_lines(fd, src_buf, src_lines);
  close(fd);
  execute_script_lines(src_lines, nlines);
  free(src_buf);
  free(src_lines);
  return 0;
}

static int builtin_echo(int argc, char *argv[], int out_fd) {
  int no_newline = 0;
  int start = 1;
  if (argc > 1 && strcmp(argv[1], "-n") == 0) {
    no_newline = 1;
    start = 2;
  }
  for (int i = start; i < argc; i++) {
    if (i > start) write(out_fd, " ", 1);
    echo_write_arg(out_fd, argv[i]);
  }
  if (!no_newline)
    write(out_fd, "\n", 1);
  return 0;
}

static int builtin_pwd(void) {
  char buf[256];
  if (getcwd(buf, sizeof(buf)) != NULL) {
    size_t len = strlen(buf);
    buf[len] = '\n';
    write(1, buf, len + 1);
  }
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

static int builtin_jobs(void) {
  for (int i = 0; i < njobs; i++) {
    const char *state = jobs[i].stopped ? "Stopped" : "Running";
    printf("[%d]+ %-8s %s\n", jobs[i].id, state, jobs[i].cmd);
  }
  return 0;
}

static int builtin_fg(int argc, char *argv[]) {
  struct job *j = NULL;
  if (argc >= 2) {
    int id = atoi(argv[1]);
    for (int i = 0; i < njobs; i++) {
      if (jobs[i].id == id) { j = &jobs[i]; break; }
    }
  } else {
    j = job_find_last_stopped();
  }
  if (!j) { printf("fg: no current job\n"); return 1; }

  pid_t pid = j->pid;
  char cmd[COMMAND_BUF_SIZE];
  strncpy(cmd, j->cmd, COMMAND_BUF_SIZE - 1);
  cmd[COMMAND_BUF_SIZE - 1] = '\0';
  job_remove(pid);

  printf("%s\n", cmd);
  ioctl(0, TCSCANON, (void *)0);
  tcsetpgrp(0, pid);
  kill(-pid, SIGCONT);

  int status = 0;
  pid_t waited;
  do { waited = waitpid(pid, &status, WUNTRACED); } while (waited < 0 && errno == EINTR);

  if (WIFSTOPPED(status)) {
    job_add(pid, cmd, 1);
    printf("\n[%d]+ Stopped  %s\n", jobs[njobs-1].id, cmd);
  } else {
    kill(-pid, SIGHUP);
  }
  tcsetpgrp(0, getpid());
  ioctl(0, TCSRAW, (void *)0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
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

  /* VAR=value [VAR=value ...] [command] — collect leading assignments.
   * If no command follows, set in shell. If a command follows, set only
   * in the environment for that child (POSIX inline env prefix). */
  {
    /* Saved env pairs to apply to child */
    static char env_names[8][MAX_VAR_NAME];
    static char env_vals[8][MAX_VAR_VALUE];
    int nenv = 0;

    const char *p = buf;
    while (*p) {
      /* skip spaces */
      while (*p == ' ' || *p == '\t') p++;
      if (*p == '\0') break;

      /* check for NAME= */
      size_t j = 0;
      const char *tok = p;
      while ((tok[j] >= 'A' && tok[j] <= 'Z') || (tok[j] >= 'a' && tok[j] <= 'z') ||
             (tok[j] >= '0' && tok[j] <= '9') || tok[j] == '_')
        j++;
      if (j == 0 || tok[j] != '=') break; /* not an assignment, stop */

      /* extract name */
      size_t nlen = j < MAX_VAR_NAME ? j : MAX_VAR_NAME - 1;
      if (nenv < 8) {
        memcpy(env_names[nenv], tok, nlen);
        env_names[nenv][nlen] = '\0';
      }

      /* extract value — advance past '=' */
      const char *raw = tok + j + 1;
      /* find end of value token (next space not in quotes) */
      size_t vlen = 0;
      int in_q = 0;
      while (raw[vlen] && (in_q || (raw[vlen] != ' ' && raw[vlen] != '\t'))) {
        if (raw[vlen] == '"') in_q = !in_q;
        vlen++;
      }
      if (nenv < 8) {
        /* strip surrounding double quotes */
        const char *vs = raw;
        size_t vl = vlen;
        if (vl >= 2 && vs[0] == '"' && vs[vl-1] == '"') { vs++; vl -= 2; }
        if (vl >= MAX_VAR_VALUE) vl = MAX_VAR_VALUE - 1;
        memcpy(env_vals[nenv], vs, vl);
        env_vals[nenv][vl] = '\0';
        nenv++;
      }
      p = raw + vlen;
    }

    /* skip spaces to see if a command follows */
    while (*p == ' ' || *p == '\t') p++;

    if (nenv > 0 && *p == '\0') {
      /* Pure assignment(s): set in shell and return */
      for (int ei = 0; ei < nenv; ei++)
        shell_var_set(env_names[ei], env_vals[ei]);
      return 0;
    }

    if (nenv > 0 && *p != '\0') {
      /* Inline env prefix: set vars in environment, run command, then restore */
      char *saved[8];
      for (int ei = 0; ei < nenv; ei++) {
        const char *old = getenv(env_names[ei]);
        saved[ei] = old ? strdup(old) : NULL;
        setenv(env_names[ei], env_vals[ei], 1);
      }
      /* Run the rest of the line as the command */
      char rest[COMMAND_BUF_SIZE];
      strncpy(rest, p, sizeof(rest) - 1);
      rest[sizeof(rest) - 1] = '\0';
      int ret = parse_and_exec(rest);
      /* Restore environment */
      for (int ei = 0; ei < nenv; ei++) {
        if (saved[ei]) { setenv(env_names[ei], saved[ei], 1); free(saved[ei]); }
        else unsetenv(env_names[ei]);
      }
      return ret;
    }
    /* nenv == 0: fall through to normal command parsing */
  }

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
      /* Double-quoted token: strip quotes, treat contents as one arg. */
      i++; /* skip opening quote */
      argv[argc++] = (char *)&buf[i];
      while (buf[i] != '"' && buf[i] != '\0') i++;
      if (buf[i] == '"') { ((char *)buf)[i] = '\0'; i++; }
    } else if (buf[i] == '\'') {
      /* Single-quoted token: strip quotes, contents are literal. */
      i++; /* skip opening quote */
      argv[argc++] = (char *)&buf[i];
      while (buf[i] != '\'' && buf[i] != '\0') i++;
      if (buf[i] == '\'') { ((char *)buf)[i] = '\0'; i++; }
    } else {
      argv[argc++] = (char *)&buf[i];
      while (buf[i] != ' ' && buf[i] != '\0') i++;
      if (buf[i] == ' ') { ((char *)buf)[i] = '\0'; i++; }
    }
  }
  argv[argc] = NULL;

  /* Scan for redirections: pull them out of argv */
  const char *redirect_out = NULL;
  const char *redirect_in  = NULL;
  const char *redirect_err = NULL;
  int append_mode = 0;
  int stderr_to_stdout = 0;
  int run_background = 0;

  for (int ri = 1; ri < argc; ri++) {
    if (strcmp(argv[ri], "&") == 0) {
      run_background = 1;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
      continue;
    }
    /* 2>&1 — redirect stderr to stdout */
    if (strcmp(argv[ri], "2>&1") == 0) {
      stderr_to_stdout = 1;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
    /* 2>/file — stderr redirect without space */
    } else if (argv[ri][0] == '2' && argv[ri][1] == '>' && argv[ri][2] != '\0' && argv[ri][2] != '&') {
      redirect_err = argv[ri] + 2;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
    /* 2> file — stderr redirect with space */
    } else if (argv[ri][0] == '2' && argv[ri][1] == '>' && argv[ri][2] == '\0' && ri + 1 < argc) {
      redirect_err = argv[ri + 1];
      for (int rj = ri; rj < argc - 2; rj++) argv[rj] = argv[rj + 2];
      argc -= 2; argv[argc] = NULL; ri--;
    /* >>/file — append without space */
    } else if (argv[ri][0] == '>' && argv[ri][1] == '>' && argv[ri][2] != '\0') {
      redirect_out = argv[ri] + 2;
      append_mode = 1;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
    /* >> file — append with space */
    } else if (argv[ri][0] == '>' && argv[ri][1] == '>' && argv[ri][2] == '\0' && ri + 1 < argc) {
      redirect_out = argv[ri + 1];
      append_mode = 1;
      for (int rj = ri; rj < argc - 2; rj++) argv[rj] = argv[rj + 2];
      argc -= 2; argv[argc] = NULL; ri--;
    /* >/file — stdout redirect without space */
    } else if (argv[ri][0] == '>' && argv[ri][1] != '>' && argv[ri][1] != '\0') {
      redirect_out = argv[ri] + 1;
      append_mode = 0;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
    /* > file — stdout redirect with space */
    } else if (argv[ri][0] == '>' && argv[ri][1] == '\0' && ri + 1 < argc) {
      redirect_out = argv[ri + 1];
      append_mode = 0;
      for (int rj = ri; rj < argc - 2; rj++) argv[rj] = argv[rj + 2];
      argc -= 2; argv[argc] = NULL; ri--;
    /* </file — stdin redirect without space */
    } else if (argv[ri][0] == '<' && argv[ri][1] != '\0') {
      redirect_in = argv[ri] + 1;
      for (int rj = ri; rj < argc - 1; rj++) argv[rj] = argv[rj + 1];
      argc--; argv[argc] = NULL; ri--;
    /* < file — stdin redirect with space */
    } else if (argv[ri][0] == '<' && argv[ri][1] == '\0' && ri + 1 < argc) {
      redirect_in = argv[ri + 1];
      for (int rj = ri; rj < argc - 2; rj++) argv[rj] = argv[rj + 2];
      argc -= 2; argv[argc] = NULL; ri--;
    }
  }

  /* Handle trailing & attached to last token, e.g. "sleep 5&" */
  if (argc > 0) {
    char *last_arg = argv[argc - 1];
    size_t last_len = strlen(last_arg);
    if (last_len > 0 && last_arg[last_len - 1] == '&') {
      run_background = 1;
      last_arg[last_len - 1] = '\0';
      if (last_len == 1) { argc--; argv[argc] = NULL; }
    }
  }

  /* Builtins */
  if (strcmp("echo", command_buf) == 0) {
    int out_fd = 1;
    if (redirect_out != NULL) {
      int flags = O_WRONLY | O_CREAT;
      flags |= append_mode ? O_APPEND : O_TRUNC;
      out_fd = open(redirect_out, flags, 0666);
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
      int fd = open(redirect_out, flags, 0666);
      if (fd >= 0) { dup2(fd, 1); close(fd); }
    }

    execve(full_path, new_argv, environ);
    printf("sh: exec: %s failed\n", argv[1]);
    return 1;
  }

  if (strcmp("cd", command_buf) == 0)
    return builtin_cd(argc, argv);
  if (strcmp("source", command_buf) == 0 || strcmp(".", command_buf) == 0)
    return builtin_source(argc, argv);
  if (strcmp("export", command_buf) == 0) {
    for (int ei = 1; ei < argc; ei++) {
      char *arg = argv[ei];
      char *eq = strchr(arg, '=');
      if (eq) {
        /* export NAME=value — set shell var and env */
        char varname[MAX_VAR_NAME];
        size_t nlen = (size_t)(eq - arg);
        if (nlen >= MAX_VAR_NAME) nlen = MAX_VAR_NAME - 1;
        memcpy(varname, arg, nlen);
        varname[nlen] = '\0';
        const char *val = eq + 1;
        shell_var_set(varname, val);
        setenv(varname, val, 1);
      } else {
        /* export NAME — push existing shell var into env */
        const char *val = shell_var_get(arg);
        if (val) setenv(arg, val, 1);
      }
    }
    return 0;
  }
  if (strcmp("unset", command_buf) == 0) {
    for (int ui = 1; ui < argc; ui++) {
      /* remove from shell var store */
      for (int vi = 0; vi < shell_var_count; vi++) {
        if (strcmp(shell_vars[vi].name, argv[ui]) == 0) {
          shell_vars[vi] = shell_vars[--shell_var_count];
          break;
        }
      }
      unsetenv(argv[ui]);
    }
    return 0;
  }
  if (strcmp("clear", command_buf) == 0) {
    write(1, "\x1b[2J\x1b[H", 8);
    return 0;
  }
  if (strcmp("true",  command_buf) == 0) return 0;
  if (strcmp("false", command_buf) == 0) return 1;
  if (strcmp("read",  command_buf) == 0) {
    int read_fd = 0;
    int opened = 0;
    if (redirect_in != NULL) {
      read_fd = open(redirect_in, O_RDONLY);
      if (read_fd < 0) { printf("sh: read: cannot open '%s'\n", redirect_in); return 1; }
      opened = 1;
    }
    char line[COMMAND_BUF_SIZE];
    int pos = 0;
    char c;
    while (pos < COMMAND_BUF_SIZE - 1) {
      int n = read(read_fd, &c, 1);
      if (n <= 0 || c == '\n') break;
      line[pos++] = c;
    }
    line[pos] = '\0';
    if (opened) close(read_fd);
    if (argc >= 2)
      shell_var_set(argv[1], line);
    return pos == 0 ? 1 : 0;
  }
  if (strcmp("pwd", command_buf) == 0)
    return builtin_pwd();
  if (strcmp("history", command_buf) == 0)
    return builtin_history();
  if (strcmp("jobs", command_buf) == 0)
    return builtin_jobs();
  if (strcmp("fg", command_buf) == 0)
    return builtin_fg(argc, argv);
  if (strcmp("which", command_buf) == 0) {
    if (argc < 2) { fprintf(stderr, "usage: which <command>\n"); return 1; }
    int ret = 0;
    for (int wi = 1; wi < argc; wi++) {
      char wp[512];
      if (resolve_command(argv[wi], wp, sizeof(wp)))
        printf("%s\n", wp);
      else {
        fprintf(stderr, "which: %s: not found\n", argv[wi]);
        ret = 1;
      }
    }
    return ret;
  }
  if (strcmp("wait", command_buf) == 0) {
    if (argc > 1) {
      pid_t target = (pid_t)atoi(argv[1]);
      /* check it's a known job first */
      int found = 0;
      for (int wi = 0; wi < njobs; wi++) {
        if (jobs[wi].pid == target) { found = 1; break; }
      }
      if (!found) {
        fprintf(stderr, "wait: %d: no such job\n", target);
        return 1;
      }
      int st = 0;
      waitpid(target, &st, 0);
      job_remove(target);
      return wait_status_to_exit(st);
    }
    /* wait for all known background jobs (not stopped ones) */
    int ret = 0;
    for (int wi = 0; wi < njobs; wi++) {
      if (jobs[wi].stopped) continue;
      int st = 0;
      waitpid(jobs[wi].pid, &st, 0);
      ret = wait_status_to_exit(st);
      jobs[wi] = jobs[--njobs];
      wi--;
    }
    return ret;
  }
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
      int fd = open(redirect_out, flags, 0666);
      if (fd < 0) { printf("sh: cannot open '%s' for writing\n", redirect_out); exit(1); }
      dup2(fd, 1);
      close(fd);
    }

    if (redirect_err != NULL) {
      int flags = O_WRONLY | O_CREAT | O_TRUNC;
      int fd = open(redirect_err, flags, 0666);
      if (fd < 0) { printf("sh: cannot open '%s' for writing\n", redirect_err); exit(1); }
      dup2(fd, 2);
      close(fd);
    } else if (stderr_to_stdout) {
      dup2(1, 2);
    }

    int ret = execve(full_path, argv, environ);
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

    if (run_background) {
      job_add(pid, command_buf, 0);
      last_bg_pid = pid;
      printf("[%d] %d\n", jobs[njobs-1].id, pid);
      ioctl(0, TCSRAW, (void *)0);
      return 0;
    }

    tcsetpgrp(0, pid);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(pid, &status, WUNTRACED); } while (waited < 0 && errno == EINTR);

    if (WIFSTOPPED(status)) {
      job_add(pid, command_buf, 1);
      printf("\n[%d]+ Stopped  %s\n", jobs[njobs-1].id, command_buf);
      tcsetpgrp(0, getpid());
      ioctl(0, TCSRAW, (void *)0);
      return 0;
    }

    /* Clean up any leftover members of the job's process group. */
    kill(-pid, SIGHUP);
    tcsetpgrp(0, getpid());
    ioctl(0, TCSRAW, (void *)0);
    return wait_status_to_exit(status);
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
  if (envp && !environ)
    environ = envp;
  if (!getenv("PATH"))
    setenv("PATH", "/bin", 1);
  if (!getenv("HOME"))
    setenv("HOME", "/", 1);
  setpgid(0, 0);
  pid_t shell_pgid = getpid();
  tcsetpgrp(0, shell_pgid);

  signal(SIGINT,  SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  history_load();
  ioctl(0, TCSRAW, (void*)0);

  char buf[COMMAND_BUF_SIZE];
  char cwd[256];
  char prompt[300];

  while (1) {
    /* Reap any finished background jobs before printing the prompt. */
    jobs_reap();

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
