#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


/* -------------------------------------------------------------------------
 * Output sink abstraction
 * fd-based sinks buffer through a stack buffer and flush on full/done.
 * string-based sinks write directly into the caller's buffer, clamped to
 * size-1, but always count the true length (like C99 snprintf requires).
 * ------------------------------------------------------------------------- */

#define FLUSH_BUF 256

typedef struct fmt_out {
  enum { OUT_FD, OUT_STR } kind;
  union {
    struct { int fd; char buf[FLUSH_BUF]; int pos; } fd;
    struct { char *buf; size_t size; int pos; } str;
  } u;
  int total;
} fmt_out;

static void out_putchar(fmt_out *o, char c) {
  o->total++;
  if (o->kind == OUT_FD) {
    o->u.fd.buf[o->u.fd.pos++] = c;
    if (o->u.fd.pos == FLUSH_BUF) {
      write(o->u.fd.fd, o->u.fd.buf, FLUSH_BUF);
      o->u.fd.pos = 0;
    }
  } else {
    if (o->u.str.pos < (int)o->u.str.size - 1)
      o->u.str.buf[o->u.str.pos] = c;
    o->u.str.pos++;
  }
}

static void out_puts(fmt_out *o, const char *s) {
  while (*s)
    out_putchar(o, *s++);
}

static void out_flush(fmt_out *o) {
  if (o->kind == OUT_FD && o->u.fd.pos > 0 && o->u.fd.fd >= 0) {
    write(o->u.fd.fd, o->u.fd.buf, (size_t)o->u.fd.pos);
    o->u.fd.pos = 0;
  } else if (o->kind == OUT_STR) {
    int end = o->u.str.pos < (int)o->u.str.size
                ? o->u.str.pos : (int)o->u.str.size - 1;
    if (o->u.str.size > 0)
      o->u.str.buf[end] = '\0';
  }
}

/* -------------------------------------------------------------------------
 * Core formatter — one place that understands % directives
 * ------------------------------------------------------------------------- */

static void fmt_uint(fmt_out *o, unsigned long long v, int base, const char *digits,
                     int width, int left_align, int zero_pad) {
  char tmp[64];
  int i = 0;
  if (v == 0) {
    tmp[i++] = '0';
  } else {
    while (v > 0) {
      tmp[i++] = digits[v % (unsigned)base];
      v /= (unsigned)base;
    }
  }
  char pad = (zero_pad && !left_align) ? '0' : ' ';
  if (!left_align)
    for (int j = i; j < width; j++)
      out_putchar(o, pad);
  while (i > 0)
    out_putchar(o, tmp[--i]);
  if (left_align)
    for (int j = i; j < width; j++)
      out_putchar(o, ' ');
}

static void fmt_str(fmt_out *o, const char *s, int width, int left_align) {
  if (!s) s = "(null)";
  int len = 0;
  for (const char *p = s; *p; p++) len++;
  if (!left_align)
    for (int i = len; i < width; i++) out_putchar(o, ' ');
  out_puts(o, s);
  if (left_align)
    for (int i = len; i < width; i++) out_putchar(o, ' ');
}

static int core_vprintf(fmt_out *o, const char *fmt, va_list args) {
  for (const char *p = fmt; *p; p++) {
    if (*p != '%') { out_putchar(o, *p); continue; }
    p++;
    if (!*p) break;

    int left_align = 0, zero_pad = 0;
    while (*p == '-' || *p == '0') {
      if (*p == '-') left_align = 1;
      if (*p == '0') zero_pad  = 1;
      p++;
    }

    int width = 0;
    while (*p >= '0' && *p <= '9')
      width = width * 10 + (*p++ - '0');

    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }

    int lng = 0;
    if (*p == 'l' && *(p+1) == 'l') { lng = 2; p += 2; }
    else if (*p == 'l')              { lng = 1; p++;    }
    else if (*p == 'z')              { lng = 1; p++;    }

    switch (*p) {
    case 'd': case 'i': {
      long long v = (lng >= 2) ? va_arg(args, long long)
                  : (lng == 1) ? (long long)va_arg(args, long)
                               : (long long)va_arg(args, int);
      char tmp[24]; int i = 0;
      unsigned long long uv;
      if (v < 0) { tmp[i++] = '-'; uv = (unsigned long long)-v; }
      else       {                  uv = (unsigned long long) v; }
      char rev[22]; int r = 0;
      if (uv == 0) { rev[r++] = '0'; }
      else { while (uv) { rev[r++] = '0' + (uv % 10); uv /= 10; } }
      while (r > 0) tmp[i++] = rev[--r];
      tmp[i] = '\0';
      char pad = (zero_pad && !left_align) ? '0' : ' ';
      int len = i;
      if (!left_align) for (int j = len; j < width; j++) out_putchar(o, pad);
      out_puts(o, tmp);
      if ( left_align) for (int j = len; j < width; j++) out_putchar(o, ' ');
      break;
    }
    case 'u': {
      unsigned long long v = (lng >= 2) ? va_arg(args, unsigned long long)
                           : (lng == 1) ? (unsigned long long)va_arg(args, unsigned long)
                                        : (unsigned long long)va_arg(args, unsigned int);
      fmt_uint(o, v, 10, "0123456789", width, left_align, zero_pad);
      break;
    }
    case 'x': {
      unsigned long long v = (lng >= 2) ? va_arg(args, unsigned long long)
                           : (lng == 1) ? (unsigned long long)va_arg(args, unsigned long)
                                        : (unsigned long long)va_arg(args, unsigned int);
      fmt_uint(o, v, 16, "0123456789abcdef", width, left_align, zero_pad);
      break;
    }
    case 'X': {
      unsigned long long v = (lng >= 2) ? va_arg(args, unsigned long long)
                           : (lng == 1) ? (unsigned long long)va_arg(args, unsigned long)
                                        : (unsigned long long)va_arg(args, unsigned int);
      fmt_uint(o, v, 16, "0123456789ABCDEF", width, left_align, zero_pad);
      break;
    }
    case 'p': {
      unsigned long long v = (unsigned long long)va_arg(args, void *);
      out_puts(o, "0x");
      fmt_uint(o, v, 16, "0123456789abcdef", 0, 0, 0);
      break;
    }
    case 's': {
      const char *s = va_arg(args, const char *);
      fmt_str(o, s, width, left_align);
      break;
    }
    case 'c':
      out_putchar(o, (char)va_arg(args, int));
      break;
    case '%':
      out_putchar(o, '%');
      break;
    default:
      out_putchar(o, '%');
      out_putchar(o, *p);
      break;
    }
  }

  out_flush(o);
  return o->total;
}

/* -------------------------------------------------------------------------
 * Public API — all thin wrappers over core_vprintf
 * ------------------------------------------------------------------------- */

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
  if (!stream || !fmt) return -1;
  if (stream->memonly) {
    fmt_out o = {
      .kind  = OUT_STR,
      .u.str = { .buf  = stream->membuf + stream->mempos,
                 .size = stream->memsize - stream->mempos,
                 .pos  = 0 },
      .total = 0,
    };
    int n = core_vprintf(&o, fmt, ap);
    stream->mempos += (size_t)o.u.str.pos < stream->memsize - stream->mempos
                      ? (size_t)o.u.str.pos : stream->memsize - stream->mempos;
    return n;
  }
  fmt_out o = { .kind = OUT_FD, .u.fd = { .fd = stream->fd, .pos = 0 }, .total = 0 };
  return core_vprintf(&o, fmt, ap);
}

int vdprintf(int fd, const char *fmt, va_list ap) {
  char buf[512];
  va_list ap2;
  va_copy(ap2, ap);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap2);
  va_end(ap2);
  if (n <= 0) return n;
  size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
  write(fd, buf, len);
  return n;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
  char dummy[1];
  fmt_out o = {
    .kind  = OUT_STR,
    .u.str = { .buf = (str && size) ? str : dummy,
               .size = (str && size) ? size : 1,
               .pos  = 0 },
    .total = 0,
  };
  return core_vprintf(&o, fmt, ap);
}

int fprintf(FILE *stream, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vfprintf(stream, fmt, ap);
  va_end(ap); return n;
}

int printf(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vfprintf(stdout, fmt, ap);
  va_end(ap); return n;
}

int vprintf(const char *fmt, va_list ap) {
  return vfprintf(stdout, fmt, ap);
}

int dprintf(int fd, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vdprintf(fd, fmt, ap);
  va_end(ap); return n;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(str, size, fmt, ap);
  va_end(ap); return n;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
  return vsnprintf(str, (size_t)-1, fmt, ap);
}

int sprintf(char *str, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vsprintf(str, fmt, ap);
  va_end(ap); return n;
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  int len = vsnprintf(NULL, 0, fmt, ap2);
  va_end(ap2);
  if (len < 0) return -1;
  *strp = malloc((size_t)len + 1);
  if (!*strp) return -1;
  return vsnprintf(*strp, (size_t)len + 1, fmt, ap);
}

int asprintf(char **strp, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vasprintf(strp, fmt, ap);
  va_end(ap); return n;
}


/* scanf family */
static int is_space(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
  const char *s = str;
  int matched = 0;

  for (; *fmt; fmt++) {
    if (is_space((unsigned char)*fmt)) {
      while (is_space((unsigned char)*s)) s++;
      continue;
    }
    if (*fmt != '%') {
      if (*s != *fmt) return matched ? matched : EOF;
      s++; continue;
    }
    fmt++;

    /* suppress assignment */
    int suppress = 0;
    if (*fmt == '*') { suppress = 1; fmt++; }

    /* width */
    int width = 0;
    while (*fmt >= '0' && *fmt <= '9')
      width = width * 10 + (*fmt++ - '0');

    if (*fmt == 'l' || *fmt == 'h') fmt++; /* ignore length modifier */

    char spec = *fmt;

    /* skip leading whitespace for most specifiers */
    if (spec != 'c' && spec != '[' && spec != 'n')
      while (is_space((unsigned char)*s)) s++;

    if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'o') {
      if (!*s) return matched ? matched : EOF;
      long val = 0;
      int neg = 0, digits = 0;
      int base = (spec == 'x') ? 16 : (spec == 'o') ? 8 : 10;
      if (spec == 'i') {
        if (s[0]=='0' && (s[1]=='x'||s[1]=='X')) { base=16; s+=2; }
        else if (s[0]=='0') { base=8; }
      }
      if (*s == '-' && spec != 'u') { neg = 1; s++; }
      else if (*s == '+') s++;
      int lim = width ? width : 0x7fffffff;
      while (lim-- && *s) {
        int d;
        if (*s>='0' && *s<='9') d = *s-'0';
        else if (base==16 && *s>='a' && *s<='f') d = *s-'a'+10;
        else if (base==16 && *s>='A' && *s<='F') d = *s-'A'+10;
        else break;
        if (d >= base) break;
        val = val * base + d; s++; digits++;
      }
      if (!digits) return matched ? matched : EOF;
      if (neg) val = -val;
      if (!suppress) {
        if (spec == 'u' || spec == 'x' || spec == 'o')
          *va_arg(ap, unsigned int *) = (unsigned int)val;
        else
          *va_arg(ap, int *) = (int)val;
        matched++;
      }
    } else if (spec == 's') {
      if (!*s) return matched ? matched : EOF;
      char *dst = suppress ? NULL : va_arg(ap, char *);
      int lim = width ? width : 0x7fffffff;
      int n = 0;
      while (lim-- && *s && !is_space((unsigned char)*s)) {
        if (!suppress) dst[n] = *s;
        n++; s++;
      }
      if (!n) return matched ? matched : EOF;
      if (!suppress) { dst[n] = '\0'; matched++; }
    } else if (spec == 'c') {
      int lim = width ? width : 1;
      char *dst = suppress ? NULL : va_arg(ap, char *);
      int n = 0;
      while (lim-- && *s) {
        if (!suppress) dst[n] = *s;
        n++; s++;
      }
      if (!n) return matched ? matched : EOF;
      if (!suppress) matched++;
    } else if (spec == 'n') {
      if (!suppress) *va_arg(ap, int *) = (int)(s - str);
    } else if (spec == '%') {
      if (*s != '%') return matched ? matched : EOF;
      s++;
    }
  }
  return matched;
}

int vfscanf(FILE *stream, const char *fmt, va_list ap) {
  /* read stream into buffer, then sscanf */
  char buf[512];
  int n = 0;
  int c;
  while (n < (int)sizeof(buf) - 1 && (c = fgetc(stream)) != EOF)
    buf[n++] = (char)c;
  buf[n] = '\0';
  return vsscanf(buf, fmt, ap);
}

int fscanf(FILE *stream, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vfscanf(stream, fmt, ap);
  va_end(ap); return n;
}
int vscanf(const char *fmt, va_list ap)  { return vfscanf(stdin, fmt, ap); }
int scanf(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vscanf(fmt, ap);
  va_end(ap); return n;
}
int sscanf(const char *str, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  int n = vsscanf(str, fmt, ap);
  va_end(ap); return n;
}
