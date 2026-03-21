#include "types.h"
#include "kernel/drivers/uart.h"
#include "platform.h"

static char hex_digit(const uint8_t c) {
    if(c < 10) return '0' + c;
    return 'A' + (c - 10);
}

void uart_putc(const char c) {
    uint64_t base = platform.uart.base;
    if (!base) base = 0x10000000;  /* Fallback to RISC-V virt default if not set */
    
    volatile uint8_t *uart_base = (volatile uint8_t *)base;
    volatile uint8_t *lsr = uart_base + 0x05;  /* Line Status Register at byte offset 0x05 */
    volatile uint8_t *thr = uart_base + 0x00; /* Transmit Holding Register at byte offset 0x00 */
    
    /* Wait up to 1000000 iterations for TX to be ready, then just write anyway */
    int timeout = 1000000;
    while(timeout-- > 0 && !(*lsr & 0x20)) {};
    *thr = c;
    return;
}

void uart_print(const char* c) {
    while(*c != '\0') {
        uart_putc(*c++);
    }
    return;
}

void uart_println(const char *s) {
    uart_print(s);
    uart_putc('\n');
}

void uart_indent(const int depth) {
    for (int i = 0; i < depth; i++)
        uart_print("  ");
}

void uart_print_hex(const uint64_t value) {
    uart_print("0x");

    for(int i = 60; i >= 0; i -= 4) {
        uart_putc(hex_digit((value >> i) & 0xF));
    }
}

void uart_print_hex_32(const uint32_t value) {
    uart_print("0x");

    for(int i = 28; i >= 0; i -= 4) {
        uart_putc(hex_digit((value >> i) & 0xF));
    }
}

void uart_print_int(int32_t value) {
    char buffer[20];
    int i = 0;

    if(value == 0) {
        uart_putc('0');
        return;
    }

    if(value < 0) {
        uart_putc('-');
        value = -value;
    }

    while(value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while(i > 0) {
        uart_putc(buffer[--i]);
    }
}

void uart_print_long_int(int64_t value) {

    char buffer[20];
    int i = 0;

    if(value == 0) {
        uart_putc('0');
        uart_putc('\n');
        return;
    }

    if(value < 0) {
        uart_putc('-');
        value = -value;
    }

    while(value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while(i > 0) {
        uart_putc(buffer[--i]);
    }

    uart_putc('\n');
}
