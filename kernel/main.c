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
  printk("About to jump to user mode:\n");
  printk("  current_task:       %p\n", current_task);
  printk("  &current_task->tf:  %p\n", &current_task->tf);
  printk("  sizeof(trap_frame): %lu bytes\n", sizeof(struct trap_frame));

  printk("\nTrap frame contents:\n");
  printk("  GPRs:\n");
  printk("    ra:  %llx  sp:  %llx  gp:  %llx  tp:  %llx\n",
         current_task->tf.ra, current_task->tf.sp, current_task->tf.gp, current_task->tf.tp);
  printk("    a0:  %llx  a1:  %llx  a2:  %llx  a7:  %llx\n",
         current_task->tf.a0, current_task->tf.a1, current_task->tf.a2, current_task->tf.a7);

  printk("  CSRs:\n");
  printk("    sepc (entry):    %llx\n", current_task->tf.sepc);
  printk("    sp (stack):      %llx\n", current_task->tf.sp);
  printk("    sstatus:         %llx\n", current_task->tf.sstatus);
  printk("    scause:          %llx\n", current_task->tf.scause);
  printk("    stval:           %llx\n", current_task->tf.stval);
  printk("    padding:         %llx\n", current_task->tf.padding);

  printk("\nDecoded sstatus (0x%llx):\n", current_task->tf.sstatus);
  printk("  SPP  (bit 8):     %d (return to %s mode)\n",
         (current_task->tf.sstatus >> 8) & 1,
         ((current_task->tf.sstatus >> 8) & 1) ? "supervisor" : "user");
  printk("  SPIE (bit 5):     %d (interrupts %s on sret)\n",
         (current_task->tf.sstatus >> 5) & 1,
         ((current_task->tf.sstatus >> 5) & 1) ? "enabled" : "disabled");
  printk("  UXL  (bits 32-33): %lld (user mode %s-bit)\n",
         (current_task->tf.sstatus >> 32) & 3,
         ((current_task->tf.sstatus >> 32) & 3) == 2 ? "64" : "other");

  printk("\nKernel SP check:\n");
  uint64_t kernel_sp;
  asm volatile("mv %0, sp" : "=r"(kernel_sp));
  printk("  Current kernel SP:   %llx\n", kernel_sp);

  printk("\nJumping to user mode now...\n");
  printk("About to call trap_return at %p with arg %p\n", trap_return, &current_task->tf);

  // Check sp one more time right before call
  uint64_t sp_before_call;
  asm volatile("mv %0, sp" : "=r"(sp_before_call));
  printk("SP right before trap_return call: %llx\n", sp_before_call);

  // Set sscratch to kernel stack top so future traps can swap to it
  printk("Setting sscratch to kernel stack top: %llx\n", current_task->kernel_context.sp);
  asm volatile("csrw sscratch, %0" :: "r"(current_task->kernel_context.sp));

  trap_return(&current_task->tf);

  printk("ERROR: trap_return returned! This should never happen\n");
  
  arch_wait();
}
