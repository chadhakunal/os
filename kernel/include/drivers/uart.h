#ifndef KERNEL_DRIVERS_UART
#define KERNEL_DRIVERS_UART

void uart_putc(char c);
void uart_print(const char* c);
void uart_print_hex(uint64_t value);
void uart_print_hex_32(uint32_t value);
void uart_print_int(int32_t value);
void uart_print_long_int(int64_t value);
void uart_println(const char *s);
void uart_indent(int depth);

#endif
