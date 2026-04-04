// OS Kernel v1.0.0

#include "cpu_idle.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "virtual_memory_init.h"

#include "lib/printk/printk.h"

/*
    TODO: NEON/FP Unit needs to be enabled
          It is currently disabled in the HW. Added -mgeneral-regs-only
          to the makefile so that the compiler doesn't use those units)
*/

void kmain(void *dtb_ptr) {
  (void)dtb_ptr;
  init_memory_info();
  printk("Kernel Started...\n");
  print_memory_info();

  init_page_allocator();
  print_pages_metadata();

  allocate_root_page_table();

  extern page_table_t *root_page_table;
  printk("Root page table allocated at: 0x%lx\n", (uint64_t)root_page_table);

  init_page_mapping();

  printk("Virtual Memory Enabled and we are still running!\n");

  uint64_t current_sp;
  asm volatile("mv %0, sp" : "=r"(current_sp));
  printk("Current SP in kmain: %llx\n", current_sp);
  printk("Kernel mapped range: %llx to %llx\n",
         KERNEL_VIRTUAL_MEMORY_BASE,
         KERNEL_VIRTUAL_MEMORY_BASE + (memory_info.kernel_end - memory_info.kernel_start));

  remove_identity_mapping();

  printk("Identity mapping removed and we are still running!\n");

  // init_process();

  arch_wait();
}
