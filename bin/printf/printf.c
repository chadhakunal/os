#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Interpret escape sequences in a format string, writing output to buf.
 * Returns number of bytes written (not including NUL).
 * Sets *stop if \c was encountered (suppress trailing newline + stop output).
 */
static int unescape(const char *s, char *buf, size_t bufsz, int *stop) {
  size_t out = 0;
  *stop = 0;
  while (*s && out < bufsz - 1) {
    if (*s != '\\') {
      buf[out++] = *s++;
      continue;
    }
    s++; /* skip backslash */
    switch (*s) {
    case 'a':  buf[out++] = '\a'; s++; break;
    case 'b':  buf[out++] = '\b'; s++; break;
    case 'f':  buf[out++] = '\f'; s++; break;
    case 'n':  buf[out++] = '\n'; s++; break;
    case 'r':  buf[out++] = '\r'; s++; break;
    case 't':  buf[out++] = '\t'; s++; break;
    case 'v':  buf[out++] = '\v'; s++; break;
    case '\\': buf[out++] = '\\'; s++; break;
    case '"':  buf[out++] = '"';  s++; break;
    case '\'': buf[out++] = '\''; s++; break;
    case 'c':
      *stop = 1;
      buf[out] = '\0';
      return (int)out;
    case 'x': {
      /* \xHH — one or two hex digits */
      s++;
      int val = 0, digits = 0;
      while (digits < 2 && ((*s >= '0' && *s <= '9') ||
                             (*s >= 'a' && *s <= 'f') ||
                             (*s >= 'A' && *s <= 'F'))) {
        val <<= 4;
        if (*s >= '0' && *s <= '9')      val |= *s - '0';
        else if (*s >= 'a' && *s <= 'f') val |= *s - 'a' + 10;
        else                              val |= *s - 'A' + 10;
        s++; digits++;
      }
      if (out < bufsz - 1) buf[out++] = (char)val;
      break;
    }
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7': {
      /* \0NNN — octal (up to 3 digits) */
      int val = 0, digits = 0;
      while (digits < 3 && *s >= '0' && *s <= '7') {
        val = val * 8 + (*s++ - '0');
        digits++;
      }
      if (out < bufsz - 1) buf[out++] = (char)val;
      break;
    }
    default:
      /* Unknown escape — pass through literally */
      buf[out++] = '\\';
      if (*s && out < bufsz - 1) buf[out++] = *s++;
      break;
    }
  }
  buf[out] = '\0';
  return (int)out;
}

/*
 * Process one %-format directive from *fmt, consuming one argument from argv.
 * Advances *fmt past the directive. Returns 0 on success, -1 if no arg.
 */
static int do_format(const char **fmt, int *argi, int argc, char **argv) {
  const char *p = *fmt; /* points just after the '%' */
  char spec[64];
  int si = 0;
  spec[si++] = '%';

  /* flags */
  while (*p == '-' || *p == '+' || *p == ' ' || *p == '0' || *p == '#')
    spec[si++] = *p++;

  /* width */
  if (*p == '*') {
    /* consume arg as width */
    int w = (*argi < argc) ? atoi(argv[(*argi)++]) : 0;
    si += snprintf(spec + si, sizeof(spec) - si, "%d", w);
    p++;
  } else {
    while (*p >= '0' && *p <= '9') spec[si++] = *p++;
  }

  /* precision */
  if (*p == '.') {
    spec[si++] = *p++;
    if (*p == '*') {
      int pr = (*argi < argc) ? atoi(argv[(*argi)++]) : 0;
      si += snprintf(spec + si, sizeof(spec) - si, "%d", pr);
      p++;
    } else {
      while (*p >= '0' && *p <= '9') spec[si++] = *p++;
    }
  }

  /* length modifier */
  if (*p == 'l' || *p == 'h' || *p == 'z') spec[si++] = *p++;
  if (*p == 'l' || *p == 'h')              spec[si++] = *p++;

  char conv = *p++;
  spec[si++] = conv;
  spec[si] = '\0';
  *fmt = p;

  const char *arg = (*argi < argc) ? argv[(*argi)++] : "";

  char out[4096];
  int olen = 0;

  switch (conv) {
  case 'd': case 'i': {
    long long v = strtoll(arg, NULL, 0);
    char lspec[68];
    int li = 0;
    for (int k = 0; spec[k] && spec[k] != conv; k++) lspec[li++] = spec[k];
    lspec[li++] = 'l'; lspec[li++] = 'l'; lspec[li++] = conv; lspec[li] = '\0';
    olen = snprintf(out, sizeof(out), lspec, v);
    break;
  }
  case 'u': case 'o': case 'x': case 'X': {
    unsigned long long v = strtoull(arg, NULL, 0);
    char lspec[68];
    int li = 0;
    for (int k = 0; spec[k] && spec[k] != conv; k++) lspec[li++] = spec[k];
    lspec[li++] = 'l'; lspec[li++] = 'l'; lspec[li++] = conv; lspec[li] = '\0';
    olen = snprintf(out, sizeof(out), lspec, v);
    break;
  }
  case 's': {
    olen = snprintf(out, sizeof(out), spec, arg);
    break;
  }
  case 'c': {
    char ch = arg[0];
    olen = snprintf(out, sizeof(out), spec, (int)(unsigned char)ch);
    break;
  }
  case 'e': case 'E': case 'f': case 'g': case 'G': {
    (void)arg;
    olen = snprintf(out, sizeof(out), "0");
    break;
  }
  case 'b': {
    /* %b — like %s but with escape interpretation (bash printf extension) */
    char unesc[4096];
    int stop = 0;
    int n = unescape(arg, unesc, sizeof(unesc), &stop);
    write(1, unesc, n);
    if (stop) return 1; /* signal \c stop */
    return 0;
  }
  case '%':
    out[0] = '%'; olen = 1;
    break;
  default:
    out[0] = '%'; out[1] = conv; olen = 2;
    break;
  }
  if (olen > 0) write(1, out, olen);
  return 0;
}

/* Advance p past one escape sequence, write the result to stdout.
 * Returns 1 if \c was seen (stop output), 0 otherwise. */
#define WCHAR(c) do { char _c = (c); write(1, &_c, 1); } while(0)

static int emit_escape(const char **p) {
  (*p)++; /* skip backslash */
  switch (**p) {
  case 'a':  WCHAR('\a'); (*p)++; break;
  case 'b':  WCHAR('\b'); (*p)++; break;
  case 'f':  WCHAR('\f'); (*p)++; break;
  case 'n':  WCHAR('\n'); (*p)++; break;
  case 'r':  WCHAR('\r'); (*p)++; break;
  case 't':  WCHAR('\t'); (*p)++; break;
  case 'v':  WCHAR('\v'); (*p)++; break;
  case '\\': WCHAR('\\'); (*p)++; break;
  case '"':  WCHAR('"');  (*p)++; break;
  case '\'': WCHAR('\''); (*p)++; break;
  case 'c':  (*p)++; return 1;
  case 'x': {
    (*p)++;
    int val = 0, digits = 0;
    while (digits < 2 && (((**p >= '0' && **p <= '9')) ||
                           (**p >= 'a' && **p <= 'f') ||
                           (**p >= 'A' && **p <= 'F'))) {
      val <<= 4;
      if (**p >= '0' && **p <= '9')      val |= **p - '0';
      else if (**p >= 'a' && **p <= 'f') val |= **p - 'a' + 10;
      else                               val |= **p - 'A' + 10;
      (*p)++; digits++;
    }
    WCHAR(val);
    break;
  }
  case '0': case '1': case '2': case '3':
  case '4': case '5': case '6': case '7': {
    int val = 0, digits = 0;
    while (digits < 3 && **p >= '0' && **p <= '7') {
      val = val * 8 + (**p - '0');
      (*p)++; digits++;
    }
    WCHAR(val);
    break;
  }
  default:
    WCHAR('\\');
    if (**p) WCHAR(*(*p)++);
    break;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: printf FORMAT [ARGUMENT...]\n");
    return 1;
  }

  const char *fmt_orig = argv[1];
  int argi = 2;

  /*
   * POSIX: if there are more arguments than format consumed, repeat the
   * format until all arguments are consumed.
   */
  do {
    const char *fmt = fmt_orig;
    int start_argi = argi;

    while (*fmt) {
      if (*fmt == '\\') {
        if (emit_escape(&fmt)) goto done;
      } else if (*fmt == '%') {
        fmt++;
        if (!*fmt) { char c = '%'; write(1, &c, 1); break; }
        if (*fmt == '%') { char c = '%'; write(1, &c, 1); fmt++; continue; }
        int r = do_format(&fmt, &argi, argc, argv);
        if (r > 0) goto done;
      } else {
        write(1, fmt, 1);
        fmt++;
      }
    }

    /* If we consumed no new arguments this pass, stop repeating. */
    if (argi == start_argi) break;

  } while (argi < argc);

done:
  return 0;
}
