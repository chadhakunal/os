// OS Kernel v1.0.0

#include "cpu_idle.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "kernel/task/elf_loader.h"
#include "virtual_memory_init.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "trap.h"
#include "kernel/drivers/uart.h"

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
  char hello[32] = "Hello World!";
  vfs_write(tty, 0, hello, 32);

  struct task_t *task = init_task();

  load_elf(task, "/bin/echo");
  printk("Loaded elf\n");
  enable_interrupts();
  uart_enable_interrupts();
  //trap_return(&task->tf);
  
  arch_wait();
}
