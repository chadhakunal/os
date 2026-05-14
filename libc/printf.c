#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#define PRINTF_BUF_SIZE 256

// Helper to add char to buffer, flushing if full
static void buf_putchar(char *buf, int *pos, char c) {
  buf[(*pos)++] = c;
  if (*pos >= PRINTF_BUF_SIZE) {
    write(1, buf, PRINTF_BUF_SIZE);
    *pos = 0;
  }
}

// Helper to add string to buffer
static void buf_puts(char *buf, int *pos, const char *s) {
  while (*s) {
    buf_putchar(buf, pos, *s++);
  }
}

// Helper to print a signed number into buffer
static void buf_print_num(char *buf, int *pos, long long num) {
  if (num < 0) {
    buf_putchar(buf, pos, '-');
    num = -num;
  }

  if (num == 0) {
    buf_putchar(buf, pos, '0');
    return;
  }

  char tmp[32];
  int i = 0;
  while (num > 0) {
    tmp[i++] = '0' + (num % 10);
    num /= 10;
  }

  // Add to buffer in reverse
  while (i > 0) {
    buf_putchar(buf, pos, tmp[--i]);
  }
}

// Helper to print an unsigned number into buffer
static void buf_print_unsigned(char *buf, int *pos, unsigned long long num) {
  if (num == 0) {
    buf_putchar(buf, pos, '0');
    return;
  }

  char tmp[32];
  int i = 0;
  while (num > 0) {
    tmp[i++] = '0' + (num % 10);
    num /= 10;
  }

  // Add to buffer in reverse
  while (i > 0) {
    buf_putchar(buf, pos, tmp[--i]);
  }
}

// Helper to print hex into buffer
static void buf_print_hex(char *buf, int *pos, unsigned long long num) {
  const char *hex = "0123456789abcdef";
  char tmp[32];
  int i = 0;

  if (num == 0) {
    buf_putchar(buf, pos, '0');
    return;
  }

  while (num > 0) {
    tmp[i++] = hex[num % 16];
    num /= 16;
  }

  // Add to buffer in reverse
  while (i > 0) {
    buf_putchar(buf, pos, tmp[--i]);
  }
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char buffer[PRINTF_BUF_SIZE];
  int pos = 0;
  int written = 0;

  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && *(p + 1)) {
      p++;

      // Check for 'll' modifier
      int is_long_long = 0;
      if (*p == 'l' && *(p + 1) == 'l') {
        is_long_long = 1;
        p += 2;
      } else if (*p == 'l') {
        // Also treat single 'l' as long long for simplicity (both are 64-bit on RISC-V)
        is_long_long = 1;
        p++;
      }

      switch (*p) {
        case 's': {
          const char *s = va_arg(args, const char *);
          if (s) {
            buf_puts(buffer, &pos, s);
          } else {
            buf_puts(buffer, &pos, "(null)");
          }
          break;
        }
        case 'd':
        case 'i': {
          if (is_long_long) {
            long long val = va_arg(args, long long);
            buf_print_num(buffer, &pos, val);
          } else {
            int val = va_arg(args, int);
            buf_print_num(buffer, &pos, val);
          }
          break;
        }
        case 'u': {
          if (is_long_long) {
            unsigned long long val = va_arg(args, unsigned long long);
            buf_print_unsigned(buffer, &pos, val);
          } else {
            unsigned int val = va_arg(args, unsigned int);
            buf_print_unsigned(buffer, &pos, val);
          }
          break;
        }
        case 'x': {
          if (is_long_long) {
            unsigned long long val = va_arg(args, unsigned long long);
            buf_print_hex(buffer, &pos, val);
          } else {
            unsigned int val = va_arg(args, unsigned int);
            buf_print_hex(buffer, &pos, val);
          }
          break;
        }
        case 'p': {
          void *ptr = va_arg(args, void *);
          buf_puts(buffer, &pos, "0x");
          buf_print_hex(buffer, &pos, (unsigned long long)ptr);
          break;
        }
        case 'c': {
          char c = (char)va_arg(args, int);
          buf_putchar(buffer, &pos, c);
          break;
        }
        case '%':
          buf_putchar(buffer, &pos, '%');
          break;
        default:
          buf_putchar(buffer, &pos, '%');
          if (is_long_long) {
            buf_putchar(buffer, &pos, 'l');
            buf_putchar(buffer, &pos, 'l');
          }
          buf_putchar(buffer, &pos, *p);
          break;
      }
    } else {
      buf_putchar(buffer, &pos, *p);
    }
  }

  // Flush any remaining data in buffer
  if (pos > 0) {
    write(1, buffer, pos);
  }

  va_end(args);
  return written;
}

// Helper to add char to a sized buffer
static void str_putchar(char *buf, size_t size, int *pos, char c) {
  if (*pos < (int)size - 1) {
    buf[(*pos)++] = c;
  }
}

// Helper to add string to a sized buffer
static void str_puts_str(char *buf, size_t size, int *pos, const char *s) {
  while (*s && *pos < (int)size - 1) {
    buf[(*pos)++] = *s++;
  }
}

// Helper to print a signed number into sized buffer
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

  while (i > 0) {
    str_putchar(buf, size, pos, tmp[--i]);
  }
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
  if (size == 0) return 0;

  va_list args;
  va_start(args, fmt);

  int pos = 0;

  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && *(p + 1)) {
      p++;

      // Check for 'll' modifier
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
          if (s) {
            str_puts_str(str, size, &pos, s);
          } else {
            str_puts_str(str, size, &pos, "(null)");
          }
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

  // Null terminate
  str[pos] = '\0';

  va_end(args);
  return pos;
}
