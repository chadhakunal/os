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

// Helper to print a number into buffer
static void buf_print_num(char *buf, int *pos, long num) {
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

// Helper to print hex into buffer
static void buf_print_hex(char *buf, int *pos, unsigned long num) {
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
          int val = va_arg(args, int);
          buf_print_num(buffer, &pos, val);
          break;
        }
        case 'u': {
          unsigned int val = va_arg(args, unsigned int);
          buf_print_num(buffer, &pos, val);
          break;
        }
        case 'x': {
          unsigned int val = va_arg(args, unsigned int);
          buf_print_hex(buffer, &pos, val);
          break;
        }
        case 'p': {
          void *ptr = va_arg(args, void *);
          buf_puts(buffer, &pos, "0x");
          buf_print_hex(buffer, &pos, (unsigned long)ptr);
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
