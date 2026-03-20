#include "types.h"
#include "drivers/uart.h"

#define UART_DEBUG_BASE 0x09000000
#define UART_DEBUG_DR ((volatile uint32_t*)(UART_DEBUG_BASE + 0x00))
#define UART_DEBUG_FR ((volatile uint32_t*)(UART_DEBUG_BASE + 0x18))
#define UART_DEBUG_CR ((volatile uint32_t*)(UART_DEBUG_BASE + 0x30))

static char hex_digit(uint8_t c) {
    if(c < 10) return '0' + c;
    return 'A' + (c - 10);
}

void uart_putc(char c) {
    while(*UART_DEBUG_FR & (1 << 5)) {};
    *UART_DEBUG_DR = c;
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

void uart_indent(int depth) {
    for (int i = 0; i < depth; i++)
        uart_print("  ");
}

void uart_print_hex(uint64_t value) {
    uart_print("0x");

    for(int i = 60; i >= 0; i -= 4) {
        uart_putc(hex_digit((value >> i) & 0xF));
    }
}

void uart_print_hex_32(uint32_t value) {
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
