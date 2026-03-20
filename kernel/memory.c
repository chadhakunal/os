#include "platform.h"
#include "memory.h"
#include "printk.h"

struct memory_info kernel_memory = {0};

void memory_init() {
  kernel_memory.kernel_start = (uint64_t)&_kernel_start;
  kernel_memory.kernel_end = (uint64_t)&_end;
  kernel_memory.kernel_size = kernel_memory.kernel_end - kernel_memory.kernel_start;

  kernel_memory.total_memory_base = platform.ram.base;
  kernel_memory.total_memory_size = platform.ram.size;
}

void print_memory() {
  printk("Memory: \n");
  printk("\tKernel Image Range: %lx -> %lx\n", kernel_memory.kernel_start, kernel_memory.kernel_end);
  printk("\tKernel Size: %llu KB\n", (kernel_memory.kernel_end - kernel_memory.kernel_start)/(1024));
  printk("\tTotal Memory Range: %lx -> %lx\n", kernel_memory.total_memory_base, kernel_memory.total_memory_base + kernel_memory.total_memory_size);
  printk("\tMemory Size: %llu MB\n", kernel_memory.total_memory_size/(1024*1024));
}
