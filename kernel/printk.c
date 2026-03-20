#include "lib/printk/printk.h"
#include "lib/printk/printk_formats.h"
#include "kernel/drivers/uart.h"
#include "types.h"
#include <stdarg.h>

static void format_print(const char **fmt_ptr, va_list *args) {
  int is_long = 0, is_short = 0;
  
  // Parse length modifiers: h, hh, l, ll
  while (**fmt_ptr == 'h' || **fmt_ptr == 'l') {
    if (**fmt_ptr == 'h') is_short++;
    if (**fmt_ptr == 'l') is_long++;
    (*fmt_ptr)++;
  }
  
  // Handle type specifier based on parsed modifiers
  switch (**fmt_ptr) {
    case 'd': { // Signed integer
      if (is_long > 1) {
        uart_print_int(va_arg(*args, int64_t));
      } else if (is_short > 1) {
        uart_print_int(va_arg(*args, int8_t));
      } else if (is_short) {
        uart_print_int(va_arg(*args, int16_t));
      } else {
        uart_print_int(va_arg(*args, int32_t));
      }
      break;
    }
    case 'u': { // Unsigned integer
      if (is_long > 1) {
        uart_print_int(va_arg(*args, uint64_t));
      } else if (is_short > 1) {
        uart_print_int(va_arg(*args, uint8_t));
      } else if (is_short) {
        uart_print_int(va_arg(*args, uint16_t));
      } else {
        uart_print_int(va_arg(*args, uint32_t));
      }
      break;
    }
    case 'x': case 'X': { // Hexadecimal
      if (is_long > 1) {
        uart_print_hex(va_arg(*args, uint64_t));
      } else {
        uart_print_hex_32(va_arg(*args, uint32_t));
      }
      break;
    }
    case 's': { // String
      char *str = va_arg(*args, char*);
      uart_print(str);
      break;
    }
    case 'c': { // Character
      char ch = (char)va_arg(*args, int);
      uart_putc(ch);
      break;
    }
    case 'p': { // Pointer
      uart_print_hex(va_arg(*args, uint64_t));
      break;
    }
    case '%': { // Escaped percent
      uart_putc('%');
      break;
    }
  }
  
  (*fmt_ptr)++; // Move past the type character
}

void printk(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  
  while (*fmt) {
    if (*fmt == '%') {
      fmt++; // Skip the %
      format_print(&fmt, &args);
    } else {
      uart_putc(*fmt);
      fmt++;
    }
  }
  
  va_end(args);
}

