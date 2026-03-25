#include "kernel/memory/memory_info.h"
#include "lib/printk/printk.h"
#include "platform.h"

memory_info_t memory_info = {0};

void init_memory_info() {
  memory_info.kernel_start = (uint64_t)&_kernel_start;
  memory_info.kernel_end = (uint64_t)&_end;
  memory_info.kernel_size = memory_info.kernel_end - memory_info.kernel_start;

  memory_info.total_memory_base = platform.ram.base;
  memory_info.total_memory_size = platform.ram.size;

  memory_info.uart_memory_address = 0x10000000;
}

void print_memory_info() {
  printk("Memory: \n");
  printk("\tKernel Image Range: %lx -> %lx\n", memory_info.kernel_start,
         memory_info.kernel_end);
  printk("\tKernel Size: %llu KB\n",
         (memory_info.kernel_end - memory_info.kernel_start) / (1024));
  printk("\tTotal Memory Range: %lx -> %lx\n", memory_info.total_memory_base,
         memory_info.total_memory_base + memory_info.total_memory_size);
  printk("\tMemory Size: %llu MB\n",
         memory_info.total_memory_size / (1024 * 1024));
}
