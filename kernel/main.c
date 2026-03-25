// OS Kernel v1.0.0

#include "cpu_idle.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"

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

  // init_process();

  printk("Virtual Memory Enabled and we are still running!\n");

  arch_wait();
}
