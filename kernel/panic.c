#include "types.h"
#include "drivers/uart.h"

void panic(const char *msg) {
  uart_println("KERNEL PANIC:");
  uart_println(msg);

  while (1) {
    asm volatile("wfi");
  }
}
