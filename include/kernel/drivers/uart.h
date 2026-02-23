#ifndef KERNEL_DRIVERS_UART
#define KERNEL_DRIVERS_UART

void uart_putc(char c);
void uart_print(char* c);
void uart_print_hex(uint64_t value);
void uart_print_hex_32(uint32_t value);
void uart_print_int(uint32_t value);

#endif
