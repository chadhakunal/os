#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#define PRINTF_BUF_SIZE 256

FILE __stdin_file  = { .fd = 0 };
FILE __stdout_file = { .fd = 1 };
FILE __stderr_file = { .fd = 2 };

static void buf_putchar(int fd, char *buf, int *pos, char c) {
  buf[(*pos)++] = c;
  if (*pos >= PRINTF_BUF_SIZE) {
    write(fd, buf, PRINTF_BUF_SIZE);
    *pos = 0;
  }
}

static void buf_puts(int fd, char *buf, int *pos, const char *s) {
  while (*s)
    buf_putchar(fd, buf, pos, *s++);
}

static void buf_print_hex(int fd, char *buf, int *pos, unsigned long long num) {
  const char *hex = "0123456789abcdef";
  char tmp[32];
  int i = 0;

  if (num == 0) {
    buf_putchar(fd, buf, pos, '0');
    return;
  }

  while (num > 0) {
    tmp[i++] = hex[num % 16];
    num /= 16;
  }

  while (i > 0)
    buf_putchar(fd, buf, pos, tmp[--i]);
}

static void buf_format_one(int fd, char *buffer, int *pos, const char **fmt_p,
                           va_list *args) {
  const char *p = *fmt_p;

  int left_align = 0, zero_pad = 0;
  while (*p == '-' || *p == '0') {
    if (*p == '-')
      left_align = 1;
    if (*p == '0')
      zero_pad = 1;
    p++;
  }

  int width = 0;
  while (*p >= '0' && *p <= '9')
    width = width * 10 + (*p++ - '0');

  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9')
      p++;
  }

  int is_long_long = 0;
  if (*p == 'l' && *(p + 1) == 'l') {
    is_long_long = 1;
    p += 2;
  } else if (*p == 'l') {
    is_long_long = 1;
    p++;
  }

  char tmp[64];
  const char *val_str = tmp;
  tmp[0] = '\0';

  switch (*p) {
  case 's': {
    const char *s = va_arg(*args, const char *);
    val_str = s ? s : "(null)";
    break;
  }
  case 'd':
  case 'i': {
    long long v = is_long_long ? va_arg(*args, long long)
                               : (long long)va_arg(*args, int);
    int i = 0;
    if (v < 0) {
      tmp[i++] = '-';
      v = -v;
    }
    if (v == 0) {
      tmp[i++] = '0';
    } else {
      char rev[32];
      int r = 0;
      while (v > 0) {
        rev[r++] = '0' + (v % 10);
        v /= 10;
      }
      while (r > 0)
        tmp[i++] = rev[--r];
    }
    tmp[i] = '\0';
    break;
  }
  case 'u': {
    unsigned long long v = is_long_long ? va_arg(*args, unsigned long long)
                                        : (unsigned long long)va_arg(*args,
                                                                       unsigned int);
    int i = 0;
    if (v == 0) {
      tmp[i++] = '0';
    } else {
      char rev[32];
      int r = 0;
      while (v > 0) {
        rev[r++] = '0' + (v % 10);
        v /= 10;
      }
      while (r > 0)
        tmp[i++] = rev[--r];
    }
    tmp[i] = '\0';
    break;
  }
  case 'x':
  case 'X': {
    unsigned long long v = is_long_long ? va_arg(*args, unsigned long long)
                                        : (unsigned long long)va_arg(*args,
                                                                       unsigned int);
    const char *hex = (*p == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
    int i = 0;
    if (v == 0) {
      tmp[i++] = '0';
    } else {
      char rev[32];
      int r = 0;
      while (v > 0) {
        rev[r++] = hex[v % 16];
        v /= 16;
      }
      while (r > 0)
        tmp[i++] = rev[--r];
    }
    tmp[i] = '\0';
    break;
  }
  case 'p': {
    unsigned long long v = (unsigned long long)va_arg(*args, void *);
    buf_puts(fd, buffer, pos, "0x");
    buf_print_hex(fd, buffer, pos, v);
    *fmt_p = p;
    return;
  }
  case 'c':
    tmp[0] = (char)va_arg(*args, int);
    tmp[1] = '\0';
    break;
  case '%':
    buf_putchar(fd, buffer, pos, '%');
    *fmt_p = p;
    return;
  default:
    buf_putchar(fd, buffer, pos, '%');
    buf_putchar(fd, buffer, pos, *p);
    *fmt_p = p;
    return;
  }

  int len = 0;
  for (const char *c = val_str; *c; c++)
    len++;

  char pad = (zero_pad && !left_align) ? '0' : ' ';
  if (!left_align)
    for (int i = len; i < width; i++)
      buf_putchar(fd, buffer, pos, pad);
  buf_puts(fd, buffer, pos, val_str);
  if (left_align)
    for (int i = len; i < width; i++)
      buf_putchar(fd, buffer, pos, ' ');

  *fmt_p = p;
}

int vfprintf(FILE *stream, const char *fmt, va_list args) {
  if (!stream || !fmt)
    return -1;

  int fd = stream->fd;
  char buffer[PRINTF_BUF_SIZE];
  int pos = 0;
  int total = 0;

  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && *(p + 1)) {
      p++;
      if (pos >= PRINTF_BUF_SIZE) {
        write(fd, buffer, PRINTF_BUF_SIZE);
        total += PRINTF_BUF_SIZE;
        pos = 0;
      }
      buf_format_one(fd, buffer, &pos, &p, &args);
    } else {
      buf_putchar(fd, buffer, &pos, *p);
    }
  }

  if (pos > 0) {
    write(fd, buffer, (size_t)pos);
    total += pos;
  }

  return total;
}

int fprintf(FILE *stream, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vfprintf(stream, fmt, args);
  va_end(args);
  return n;
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int n = vfprintf(stdout, fmt, args);
  va_end(args);
  return n;
}

int fputs(const char *s, FILE *stream) {
  if (!s || !stream)
    return EOF;

  size_t len = 0;
  while (s[len])
    len++;

  if (len == 0)
    return 0;

  ssize_t n = write(stream->fd, s, len);
  if (n < 0 || (size_t)n != len)
    return EOF;

  return 0;
}

int fputc(int c, FILE *stream) {
  if (!stream)
    return EOF;

  unsigned char ch = (unsigned char)c;
  if (write(stream->fd, &ch, 1) != 1)
    return EOF;

  return (unsigned char)c;
}

static void str_putchar(char *buf, size_t size, int *pos, char c) {
  if (*pos < (int)size - 1)
    buf[(*pos)++] = c;
}

static void str_puts_str(char *buf, size_t size, int *pos, const char *s) {
  while (*s && *pos < (int)size - 1)
    buf[(*pos)++] = *s++;
}

static void str_print_num(char *buf, size_t size, int *pos, long long num) {
  if (num < 0) {
    str_putchar(buf, size, pos, '-');
    num = -num;
  }

  if (num == 0) {
    str_putchar(buf, size, pos, '0');
    return;
  }

  char tmp[32];
  int i = 0;
  while (num > 0) {
    tmp[i++] = '0' + (num % 10);
    num /= 10;
  }

  while (i > 0)
    str_putchar(buf, size, pos, tmp[--i]);
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
  if (size == 0)
    return 0;

  va_list args;
  va_start(args, fmt);

  int pos = 0;

  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && *(p + 1)) {
      p++;

      int is_long_long = 0;
      if (*p == 'l' && *(p + 1) == 'l') {
        is_long_long = 1;
        p += 2;
      } else if (*p == 'l') {
        is_long_long = 1;
        p++;
      }

      switch (*p) {
      case 's': {
        const char *s = va_arg(args, const char *);
        if (s)
          str_puts_str(str, size, &pos, s);
        else
          str_puts_str(str, size, &pos, "(null)");
        break;
      }
      case 'd':
      case 'i': {
        if (is_long_long) {
          long long val = va_arg(args, long long);
          str_print_num(str, size, &pos, val);
        } else {
          int val = va_arg(args, int);
          str_print_num(str, size, &pos, val);
        }
        break;
      }
      case '%':
        str_putchar(str, size, &pos, '%');
        break;
      default:
        str_putchar(str, size, &pos, '%');
        if (is_long_long) {
          str_putchar(str, size, &pos, 'l');
          str_putchar(str, size, &pos, 'l');
        }
        str_putchar(str, size, &pos, *p);
        break;
      }
    } else {
      str_putchar(str, size, &pos, *p);
    }
  }

  str[pos] = '\0';

  va_end(args);
  return pos;
}
