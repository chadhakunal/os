#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

// Simple helper to write a string
static int puts_noln(const char *s) {
  int count = 0;
  while (s[count]) count++;
  return write(1, s, count);
}

// Simple helper to write a single char
static void putchar(char c) {
  write(1, &c, 1);
}

// Simple helper to print a number
static void print_num(long num) {
  if (num < 0) {
    putchar('-');
    num = -num;
  }

  if (num == 0) {
    putchar('0');
    return;
  }

  char buf[32];
  int i = 0;
  while (num > 0) {
    buf[i++] = '0' + (num % 10);
    num /= 10;
  }

  // Print in reverse
  while (i > 0) {
    putchar(buf[--i]);
  }
}

// Simple helper to print hex
static void print_hex(unsigned long num) {
  const char *hex = "0123456789abcdef";
  char buf[32];
  int i = 0;

  if (num == 0) {
    putchar('0');
    return;
  }

  while (num > 0) {
    buf[i++] = hex[num % 16];
    num /= 16;
  }

  // Print in reverse
  while (i > 0) {
    putchar(buf[--i]);
  }
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  int written = 0;

  for (const char *p = fmt; *p; p++) {
    if (*p == '%' && *(p + 1)) {
      p++;
      switch (*p) {
        case 's': {
          const char *s = va_arg(args, const char *);
          if (s) {
            written += puts_noln(s);
          } else {
            written += puts_noln("(null)");
          }
          break;
        }
        case 'd':
        case 'i': {
          int val = va_arg(args, int);
          print_num(val);
          written += 1; // Approximation
          break;
        }
        case 'u': {
          unsigned int val = va_arg(args, unsigned int);
          print_num(val);
          written += 1;
          break;
        }
        case 'x': {
          unsigned int val = va_arg(args, unsigned int);
          print_hex(val);
          written += 1;
          break;
        }
        case 'p': {
          void *ptr = va_arg(args, void *);
          puts_noln("0x");
          print_hex((unsigned long)ptr);
          written += 3;
          break;
        }
        case 'c': {
          char c = (char)va_arg(args, int);
          putchar(c);
          written += 1;
          break;
        }
        case '%':
          putchar('%');
          written += 1;
          break;
        default:
          putchar('%');
          putchar(*p);
          written += 2;
          break;
      }
    } else {
      putchar(*p);
      written += 1;
    }
  }

  va_end(args);
  return written;
}
