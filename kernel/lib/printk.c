#include "lib/printk/printk.h"
#include "lib/printk/printk_formats.h"
#include "kernel/drivers/uart.h"
#include "types.h"
#include <stdarg.h>

/* -------------------------------------------------------------------------
 * Kernel log ring buffer
 * ---------------------------------------------------------------------- */
static char  klog_buf[KLOG_SIZE];
static size_t klog_write = 0; /* total bytes ever written (not wrapped) */

static void klog_putc(char c) {
  klog_buf[klog_write % KLOG_SIZE] = c;
  klog_write++;
}

size_t klog_len(void) {
  return klog_write < KLOG_SIZE ? klog_write : KLOG_SIZE;
}

/*
 * Read up to `size` bytes of the log starting at logical `offset`
 * (0 = oldest available byte). Returns the number of bytes copied.
 */
size_t klog_read(char *buf, size_t size, size_t offset) {
  size_t total = klog_len();
  if (offset >= total) return 0;
  size_t available = total - offset;
  size_t n = size < available ? size : available;

  /* If the buffer hasn't wrapped yet, data starts at index 0. */
  if (klog_write <= KLOG_SIZE) {
    for (size_t i = 0; i < n; i++)
      buf[i] = klog_buf[offset + i];
  } else {
    /* Buffer has wrapped: oldest byte is at klog_write % KLOG_SIZE. */
    size_t start = (klog_write % KLOG_SIZE + offset) % KLOG_SIZE;
    for (size_t i = 0; i < n; i++)
      buf[i] = klog_buf[(start + i) % KLOG_SIZE];
  }
  return n;
}

/* Emit a single character to both UART and the kernel log buffer. */
static void emit(char c) {
  uart_putc(c);
  klog_putc(c);
}

/* Emit a NUL-terminated string to both outputs. */
static void emit_str(const char *s) {
  for (; *s; s++) emit(*s);
}

/* Render an unsigned 64-bit integer into scratch[], return length. */
static int render_uint64(char *scratch, uint64_t v, int base) {
  static const char *digits = "0123456789abcdef";
  int i = 0;
  if (v == 0) { scratch[i++] = '0'; return i; }
  while (v > 0) { scratch[i++] = digits[v % (uint64_t)base]; v /= (uint64_t)base; }
  /* reverse */
  for (int a = 0, b = i - 1; a < b; a++, b--) {
    char t = scratch[a]; scratch[a] = scratch[b]; scratch[b] = t;
  }
  return i;
}

static void format_print(const char **fmt_ptr, va_list *args) {
  int is_long = 0, is_short = 0;

  while (**fmt_ptr == 'h' || **fmt_ptr == 'l') {
    if (**fmt_ptr == 'h') is_short++;
    if (**fmt_ptr == 'l') is_long++;
    (*fmt_ptr)++;
  }

  char scratch[32];
  int  len;

  switch (**fmt_ptr) {
    case 'd': {
      int64_t v = (is_long > 1) ? va_arg(*args, int64_t)
                : (is_short > 1) ? (int8_t)va_arg(*args, int)
                : (is_short)     ? (int16_t)va_arg(*args, int)
                                 : (int32_t)va_arg(*args, int32_t);
      if (v < 0) { emit('-'); v = -v; }
      len = render_uint64(scratch, (uint64_t)v, 10);
      for (int i = 0; i < len; i++) emit(scratch[i]);
      break;
    }
    case 'u': {
      uint64_t v = (is_long > 1) ? va_arg(*args, uint64_t)
                 : (is_short > 1) ? (uint8_t)va_arg(*args, unsigned int)
                 : (is_short)     ? (uint16_t)va_arg(*args, unsigned int)
                                  : (uint32_t)va_arg(*args, uint32_t);
      len = render_uint64(scratch, v, 10);
      for (int i = 0; i < len; i++) emit(scratch[i]);
      break;
    }
    case 'x': case 'X': case 'p': {
      uint64_t v = (is_long > 1 || **fmt_ptr == 'p')
                   ? va_arg(*args, uint64_t)
                   : (uint32_t)va_arg(*args, uint32_t);
      emit_str("0x");
      len = render_uint64(scratch, v, 16);
      for (int i = 0; i < len; i++) emit(scratch[i]);
      break;
    }
    case 's': {
      char *str = va_arg(*args, char *);
      emit_str(str ? str : "(null)");
      break;
    }
    case 'c': {
      emit((char)va_arg(*args, int));
      break;
    }
    case '%': {
      emit('%');
      break;
    }
  }

  (*fmt_ptr)++;
}

/*
 * Snapshot the ring buffer write position before and after each format
 * specifier so we can mirror whatever the uart_* helpers emitted into
 * klog_buf.  We do this by capturing the UART output via a small local
 * buffer approach: we temporarily redirect all uart_putc calls through
 * a wrapper at the vprintk loop level for plain characters, and rely on
 * the per-specifier klog_putc calls added to 's' and 'c' cases above
 * for those types.  Numeric/pointer specifiers are captured below using
 * a scratch buffer rendered the same way as libc printf.
 */
void vprintk(const char *fmt, va_list args) {
  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      format_print(&fmt, &args);
    } else {
      uart_putc(*fmt);
      klog_putc(*fmt);
      fmt++;
    }
  }
}

void printk(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintk(fmt, args);
  va_end(args);
}

