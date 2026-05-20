#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Forward declaration for recursive expression evaluation. */
static int eval(int argc, char *argv[], int *pos);

static int str_to_int(const char *s) { return atoi(s); }

/* Evaluate a primary expression starting at argv[*pos]. */
static int eval_primary(int argc, char *argv[], int *pos) {
  if (*pos >= argc) return 0;

  const char *tok = argv[*pos];

  /* Unary operators */
  if (strcmp(tok, "!") == 0) {
    (*pos)++;
    return !eval_primary(argc, argv, pos);
  }

  if (strcmp(tok, "(") == 0) {
    (*pos)++;
    int v = eval(argc, argv, pos);
    if (*pos < argc && strcmp(argv[*pos], ")") == 0)
      (*pos)++;
    return v;
  }

  /* Flag tests: -e, -f, -d, -r, -w, -x, -s, -z, -n */
  if (tok[0] == '-' && tok[1] && !tok[2]) {
    char flag = tok[1];
    if (flag == 'z' || flag == 'n') {
      (*pos)++;
      const char *s = (*pos < argc) ? argv[(*pos)++] : "";
      return flag == 'z' ? (strlen(s) == 0) : (strlen(s) > 0);
    }
    /* File tests */
    (*pos)++;
    const char *path = (*pos < argc) ? argv[(*pos)++] : "";
    struct stat st;
    int exists = (stat(path, &st) == 0);
    switch (flag) {
      case 'e': return exists;
      case 'f': return exists && S_ISREG(st.st_mode);
      case 'd': return exists && S_ISDIR(st.st_mode);
      case 'r': return exists && (st.st_mode & 0444);
      case 'w': return exists && (st.st_mode & 0222);
      case 'x': return exists && (st.st_mode & 0111);
      case 's': return exists && st.st_size > 0;
      default:  return 0;
    }
  }

  /* Binary expressions: string and integer comparisons */
  if (*pos + 2 < argc) {
    const char *a  = argv[(*pos)++];
    const char *op = argv[(*pos)++];
    const char *b  = argv[(*pos)++];

    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0)
      return strcmp(a, b) == 0;
    if (strcmp(op, "!=") == 0)
      return strcmp(a, b) != 0;
    if (strcmp(op, "-eq") == 0) return str_to_int(a) == str_to_int(b);
    if (strcmp(op, "-ne") == 0) return str_to_int(a) != str_to_int(b);
    if (strcmp(op, "-lt") == 0) return str_to_int(a) <  str_to_int(b);
    if (strcmp(op, "-le") == 0) return str_to_int(a) <= str_to_int(b);
    if (strcmp(op, "-gt") == 0) return str_to_int(a) >  str_to_int(b);
    if (strcmp(op, "-ge") == 0) return str_to_int(a) >= str_to_int(b);

    /* Unknown operator — treat as non-empty string = true */
    return strlen(a) > 0;
  }

  /* Single token: true iff non-empty */
  (*pos)++;
  return strlen(tok) > 0;
}

/* Evaluate -a / -o conjunctions. */
static int eval(int argc, char *argv[], int *pos) {
  int v = eval_primary(argc, argv, pos);

  while (*pos < argc) {
    const char *op = argv[*pos];
    if (strcmp(op, "-a") == 0) {
      (*pos)++;
      int r = eval_primary(argc, argv, pos);
      v = v && r;
    } else if (strcmp(op, "-o") == 0) {
      (*pos)++;
      int r = eval_primary(argc, argv, pos);
      v = v || r;
    } else {
      break;
    }
  }
  return v;
}

int main(int argc, char *argv[]) {
  /* When invoked as "[", last argument must be "]". */
  int is_bracket = (argv[0][0] == '[');
  int end = argc;
  if (is_bracket) {
    if (argc < 2 || strcmp(argv[argc - 1], "]") != 0) {
      fprintf(stderr, "[: missing ]\n");
      return 2;
    }
    end = argc - 1;
  }

  if (end == 1) return 1; /* no expression = false */

  int pos = 1;
  int result = eval(end - 1, argv + 1, &pos);
  /* Invert: test returns 0 (success) when expression is true. */
  return result ? 0 : 1;
}
