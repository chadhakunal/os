// OS Kernel v1.0.0

#include "arch/riscv64/cpu_idle.h"
#include "platform.h"
#include "types.h"

#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/task.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/sbi.h"
#include "kernel/drivers/uart.h"
#include "kernel/drivers/tty.h"
#include "kernel/drivers/rtc/rtc.h"
#include "kernel/drivers/plic.h"
#include "kernel/time/timer.h"
#include "kernel/task/schedule.h"
#include "kernel/drivers/virtio-blk.h"

#define DEBUG 0
#include "lib/printk/printk.h"
#include "kernel/signal_jump_point.h"
#include "arch/riscv64/syscalls/syscalls.h"


void kmain(void *dtb_ptr) {
  (void)dtb_ptr;
  printk("Kernel Started...\n");
  struct task_t _t;
  printk("sizeof(task_t)=%llu pid_off=%llu satp_off=%llu\n",
         (uint64_t)sizeof(struct task_t),
         (uint64_t)((char*)&_t.pid - (char*)&_t),
         (uint64_t)((char*)&_t.mm_struct.root_satp - (char*)&_t));
  init_memory_info();
  print_memory_info();
  init_page_allocator();
  print_pages_metadata();
  printk("boot: init_kernel_page_mapping\n");
  init_kernel_page_mapping();

  // Jump to higher-half execution
  uint64_t offset = KERNEL_VIRT_OFFSET;
  asm volatile("la t0, 1f\n"
               "add t0, t0, %[off]\n"
               "jr t0\n"
               "1:\n"
               :
               : [off] "r"(offset)
               : "t0", "memory");

  printk("boot: remove_identity_mapping\n");
  remove_identity_mapping();

  printk("Initialized Paging, Virtual Memory and Moved Kernel to Upper Region\n");

  init_signal_jump_point();

  init_trap_handler();
  printk("Initialized Trap Handler\n");

  rtc_init();
  printk("Initialized RTC: Current time: %llu seconds since epoch\n", rtc_read_time_sec());
  getrandom_init();

  tty_init();
  printk("Initialized TTY driver\n");

  plic_init();
  printk("Initialized PLIC\n");

  virtio_blk_init();
  printk("Initialized virtio-blk\n");

  vfs_init();
  printk("Initialized vfs and mounted tarfs\n");

  // printk("Starting read of /etc/rc\n");
  // // Test vfs_read with a loop
  // struct file_t *file;
  // int64_t ret = vfs_open("/etc/rc", O_RDONLY, &file);
  // if (ret == 0 && file != NULL) {
  //   printk("Reading /etc/rc in chunks:\n");
  //   char buffer[32];
  //   size_t offset = 0;
  //   int64_t bytes_read;
  //
  //   while ((bytes_read = vfs_read(file, offset, buffer, sizeof(buffer) - 1)) > 0) {
  //     buffer[bytes_read] = '\0';  // Null terminate
  //     printk("%s", buffer);
  //     offset += bytes_read;
  //
  //     if (bytes_read < sizeof(buffer) - 1) {
  //       break;  // EOF reached
  //     }
  //   }
  //   printk("\n--- End of file ---\n");
  // }

  // struct file_t *tty;
  // ret = vfs_open("/dev/tty", O_RDWR, &tty);
  // char hello[32] = "Hello World!\n";
  // vfs_write(tty, 0, hello, 32);

  create_idle_task();
  printk("Created idle task (PID 0)\n");

  create_init_process();
  printk("Created init process from /bin/init (PID 1)\n");
  debugk("Size of task struct: %lld\n", sizeof(struct task_t));

  init_virtual_time();
  init_timer();
  printk("Initialized Timer\n");

  init_scheduler();

  uart_enable_interrupts();
  printk("Enabled uart interrupts\n");

  extern void start_init_task(struct trap_frame *tf, uint64_t kernel_sp);
  start_init_task(&current_task->tf, current_task->kernel_context.sp);
  //
  // printk("ERROR: start_init_task returned! This should never happen\n");
  
  arch_wait();
}
