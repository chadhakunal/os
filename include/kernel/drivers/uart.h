#ifndef KERNEL_DRIVERS_UART
#define KERNEL_DRIVERS_UART

#include "types.h"

void uart_putc(const char c);
void uart_print(const char *c);
void uart_print_hex(const uint64_t value);
void uart_print_hex_32(const uint32_t value);
void uart_print_int(int32_t value);
void uart_print_long_int(int64_t value);
void uart_println(const char *s);
void uart_indent(const int depth);

#endif
