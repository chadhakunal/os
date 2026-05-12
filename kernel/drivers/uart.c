#include "kernel/drivers/uart.h"
#include "platform.h"
#include "types.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/drivers/tty.h"

static char hex_digit(const uint8_t c) {
  if (c < 10)
    return '0' + c;
  return 'A' + (c - 10);
}

volatile uint8_t *uart_get_base(void) {
  /* After virtual memory is enabled, access UART through virtual MMIO address.
     Before that, use physical address directly. */
  extern int _virtual_memory_enabled;

  uint64_t uart_phys = platform.uart.base;
  if (uart_phys == 0) {
    uart_phys = 0x10000000; /* fallback default */
  }

  /* If virtual memory is enabled, map through MMIO virtual base */
  if (_virtual_memory_enabled) {
    uint64_t uart_phys_aligned = uart_phys & ~0xFFFULL;
    return (volatile uint8_t *)MMIO_PHYS_TO_VIRT(uart_phys_aligned);
  }

  /* Before MMU: use physical address directly */
  return (volatile uint8_t *)uart_phys;
}

void uart_putc(const char c) {
  /* RISC-V virt UART (NS16550A) */
  volatile uint8_t *uart = uart_get_base();
  uint32_t offset = platform.uart.base & 0xFFFULL;

  #define LSR_OFFSET 5
  #define LSR_THRE (1 << 5)  /* Transmitter Holding Register Empty */

  /* Wait for transmitter to be ready */
  while (!(uart[offset + LSR_OFFSET] & LSR_THRE)) {
    /* Busy wait - transmitter is full */
  }

  /* Write to THR (offset 0x00) */
  uart[offset] = c;
  return;
}

void uart_print(const char *c) {
  while (*c != '\0') {
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

  for (int i = 60; i >= 0; i -= 4) {
    uart_putc(hex_digit((value >> i) & 0xF));
  }
}

void uart_print_hex_32(const uint32_t value) {
  uart_print("0x");

  for (int i = 28; i >= 0; i -= 4) {
    uart_putc(hex_digit((value >> i) & 0xF));
  }
}

void uart_print_int(int32_t value) {
  char buffer[20];
  int i = 0;

  if (value == 0) {
    uart_putc('0');
    return;
  }

  if (value < 0) {
    uart_putc('-');
    value = -value;
  }

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i > 0) {
    uart_putc(buffer[--i]);
  }
}

void uart_print_long_int(int64_t value) {

  char buffer[20];
  int i = 0;

  if (value == 0) {
    uart_putc('0');
    return;
  }

  if (value < 0) {
    uart_putc('-');
    value = -value;
  }

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i > 0) {
    uart_putc(buffer[--i]);
  }
}

//Must be called after virtual memory is enabled
void uart_enable_interrupts(void) {
  // Get UART base (uses virtual address after VM enabled)
  volatile uint8_t *uart = uart_get_base();
  uint32_t offset = platform.uart.base & 0xFFFULL;

  #define IER_OFFSET 1  // Interrupt Enable Register
  #define FCR_OFFSET 2  // FIFO Control Register

  /* Enable receive data available interrupt (bit 0 of IER) */
  uart[offset + IER_OFFSET] = 0x01;

  /* Enable and clear FIFOs */
  uart[offset + FCR_OFFSET] = 0x07;

  #define PLIC_BASE       0x0c000000UL
  #define UART_IRQ        10  // UART interrupt ID on QEMU virt

  volatile uint32_t *plic_priority = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE);

  plic_priority[UART_IRQ] = 1;

  volatile uint32_t *plic_s_enable = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE + 0x2080);
  plic_s_enable[UART_IRQ / 32] |= (1U << (UART_IRQ % 32));

  volatile uint32_t *plic_s_threshold = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE + 0x201000);
  *plic_s_threshold = 0;
}

int uart_getc(char *buffer, int max_len) {
  volatile uint8_t *uart = uart_get_base();
  uint32_t offset = platform.uart.base & 0xFFFULL;

  #define LSR_OFFSET 5
  #define RBR_OFFSET 0

  int count = 0;

  while (count < max_len && (uart[offset + LSR_OFFSET] & 0x01)) {
    buffer[count++] = uart[offset + RBR_OFFSET];
  }

  return count;
}

void handle_uart_interrupt() {
  #define PLIC_BASE       0x0c000000UL
  #define PLIC_S_CLAIM    (PLIC_BASE + 0x201004)

  volatile uint32_t *plic_claim = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_S_CLAIM);
  uint32_t irq = *plic_claim;

  char buff[16];
  uint64_t size = uart_getc(buff, 16);
  tty_receive(buff, size);

  *plic_claim = irq;
}
