#include "platform.h"
#include "kernel/drivers/uart.h"
#include "kernel/memory.h"

struct memory_info kernel_memory = {0};

void memory_init() {
  kernel_memory.kernel_start = (uint64_t)&_kernel_start;
  kernel_memory.kernel_end = (uint64_t)&_end;
  kernel_memory.kernel_size = kernel_memory.kernel_end - kernel_memory.kernel_start;

  kernel_memory.total_memory_base = platform.ram.base;
  kernel_memory.total_memory_size = platform.ram.size;
}

void print_memory() {
    uart_print("Memory: \n");
    uart_print_hex(kernel_memory.kernel_start);
    uart_print_hex(kernel_memory.kernel_end);
    uart_print_hex(kernel_memory.total_memory_base);
    uart_print_hex(kernel_memory.total_memory_size);
}
