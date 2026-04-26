// OS Kernel v1.0.0

#include "arch/riscv64/cpu_idle.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "kernel/task/elf_loader.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/trap.h"
#include "kernel/drivers/uart.h"
#include "kernel/drivers/tty.h"

#include "lib/printk/printk.h"

/*
    TODO: NEON/FP Unit needs to be enabled
          It is currently disabled in the HW. Added -mgeneral-regs-only
          to the makefile so that the compiler doesn't use those units)
*/

void kmain(void *dtb_ptr) {
  (void)dtb_ptr;
  printk("Kernel Started...\n");
  init_memory_info();
  print_memory_info();
  init_page_allocator();
  print_pages_metadata();

  init_kernel_page_mapping();

  /* Jump to higher-half execution */
  uint64_t offset = KERNEL_VIRT_OFFSET;
  asm volatile("la t0, 1f\n"
               "add t0, t0, %[off]\n"
               "jr t0\n"
               "1:\n"
               :
               : [off] "r"(offset)
               : "t0", "memory");

  uint64_t current_sp, current_pc;
  asm volatile("mv %0, sp" : "=r"(current_sp));
  asm volatile("auipc %0, 0" : "=r"(current_pc));
  remove_identity_mapping();

  printk("Initialized Paging, Virtual Memory and Moved Kernel to Upper Region\n");

  init_trap_handler();
  printk("Initialized Trap Handler\n");
  // init_process();
  //struct elf_file *parsed = parse_elf_file((void *)0x000001);
  tty_init();
  printk("Initialized TTY driver\n");
  vfs_init();
  printk("Initialized vfs and mounted tarfs\n");

  printk("Starting read of /etc/rc\n");
  // Test vfs_read with a loop
  struct file_t *file;
  int64_t ret = vfs_open("/etc/rc", O_RDONLY, &file);
  if (ret == 0 && file != NULL) {
    printk("Reading /etc/rc in chunks:\n");
    char buffer[32];
    size_t offset = 0;
    int64_t bytes_read;

    while ((bytes_read = vfs_read(file, offset, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';  // Null terminate
      printk("%s", buffer);
      offset += bytes_read;

      if (bytes_read < sizeof(buffer) - 1) {
        break;  // EOF reached
      }
    }
    printk("\n--- End of file ---\n");
  }

  struct file_t *tty;
  ret = vfs_open("/dev/tty", O_RDWR, &tty);
  char hello[32] = "Hello World!\n";
  vfs_write(tty, 0, hello, 32);

  // enable_interrupts();
  // uart_enable_interrupts();

  create_init_process();
  printk("Done creating init_process\n");

  printk("Setting sscratch to kernel stack top: %llx\n", current_task->kernel_context.sp);
  asm volatile("csrw sscratch, %0" :: "r"(current_task->kernel_context.sp));

  // Switch to user's page table
  uint64_t user_satp = (8ULL << 60) | ((uint64_t)current_task->mm_struct.root_satp >> 12);
  printk("Switching to user page table: satp=0x%llx (root_satp=%p)\n",
         user_satp, current_task->mm_struct.root_satp);
  asm volatile("sfence.vma zero, zero");
  asm volatile("csrw satp, %0" :: "r"(user_satp) : "memory");
  asm volatile("sfence.vma zero, zero");
  asm volatile("fence.i");
  printk("Swapped page table, trap return to current_task\n");
  trap_return(&current_task->tf);

  printk("ERROR: trap_return returned! This should never happen\n");
  
  arch_wait();
}
