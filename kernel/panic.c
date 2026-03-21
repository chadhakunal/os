#include "types.h"
#include "arch/cpu_idle.h"
#include "kernel/drivers/uart.h"

void panic(const char *msg) {
  uart_println("KERNEL PANIC:");
  uart_println(msg);
  arch_wait();
}
