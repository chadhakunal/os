#include "kernel/drivers/uart.h"
#include "platform.h"
#include "types.h"
#include "arch/riscv64/virtual_memory_init.h"

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
    uart_putc('\n');
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

  uart_putc('\n');
}

/* Enable UART receive interrupts and configure PLIC
 * IMPORTANT: Must be called AFTER virtual memory is enabled */
void uart_enable_interrupts(void) {
  /* Get UART base (uses virtual address after VM enabled) */
  volatile uint8_t *uart = uart_get_base();
  uint32_t offset = platform.uart.base & 0xFFFULL;

  /* NS16550A UART register offsets */
  #define IER_OFFSET 1  /* Interrupt Enable Register */
  #define FCR_OFFSET 2  /* FIFO Control Register */

  /* Enable receive data available interrupt (bit 0 of IER) */
  uart[offset + IER_OFFSET] = 0x01;

  /* Enable and clear FIFOs */
  uart[offset + FCR_OFFSET] = 0x07;

  /* Configure PLIC (Platform-Level Interrupt Controller)
   * QEMU virt machine PLIC mapping */
  #define PLIC_BASE       0x0c000000UL
  #define UART_IRQ        10  /* UART interrupt ID on QEMU virt */

  /* All PLIC accesses use virtual addresses */
  volatile uint32_t *plic_priority = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE);

  /* Set UART interrupt priority to 1 (0 = disabled, 1-7 = enabled with priority) */
  plic_priority[UART_IRQ] = 1;

  /* Enable UART interrupt for supervisor mode on hart 0
   * Context 1 = Supervisor mode on hart 0
   * Enable registers start at offset 0x2000, each context has 0x80 bytes
   * Context 1 enable base = 0x2080 */
  volatile uint32_t *plic_s_enable = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE + 0x2080);
  plic_s_enable[UART_IRQ / 32] |= (1U << (UART_IRQ % 32));

  /* Set priority threshold to 0 (accept all non-zero priority interrupts)
   * Context 1 threshold at offset 0x201000 */
  volatile uint32_t *plic_s_threshold = (volatile uint32_t *)MMIO_PHYS_TO_VIRT(PLIC_BASE + 0x201000);
  *plic_s_threshold = 0;

  uart_println("UART interrupts enabled (IER=0x01, PLIC configured for IRQ 10)");
}
