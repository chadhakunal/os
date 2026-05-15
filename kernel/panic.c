#include "types.h"
#include "arch/riscv64/cpu_idle.h"
#include "lib/printk/printk.h"
#include <stdarg.h>

void panic(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printk("KERNEL PANIC: ");
  vprintk(fmt, args);
  printk("\n");
  va_end(args);
  arch_wait();
}
