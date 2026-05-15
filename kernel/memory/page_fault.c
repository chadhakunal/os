#define DEBUG 1
#include "kernel/memory/page_fault.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/memory/page_tables.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"
#include "kernel/panic.h"


static uint64_t vma_to_pte_flags(uint64_t vm_flags) {
  uint64_t pte_flags = PTE_VALID | PTE_A | PTE_U;

  if (vm_flags & VM_READ)  pte_flags |= PTE_R;
  if (vm_flags & VM_WRITE) pte_flags |= PTE_W | PTE_D;
  if (vm_flags & VM_EXEC)  pte_flags |= PTE_X;

  return pte_flags;
}

// Helper to handle SIGSEGV when page fault cannot be resolved
// If in supervisor mode, return to user space for signal delivery (never returns)
static void send_sigsegv_and_abort_syscall(void) {
  extern void trap_return(struct trap_frame *tf);

  debugk("Sending SIGSEGV to PID %llu\n", current_task->pid);
  send_signal(current_task, SIGSEGV);

  // If in supervisor mode (during syscall), return to user space for signal delivery
  debugk("current_task->tf.sstatus = 0x%llx, SPP bit = %d\n",
         current_task->tf.sstatus, !!(current_task->tf.sstatus & SSTATUS_SPP));

  if (current_task->tf.sstatus & SSTATUS_SPP) {
    debugk("Detected SPP=1 in saved tf, calling trap_return to user mode\n");
    debugk("User mode PC = 0x%llx, SP = 0x%llx\n", current_task->tf.sepc, current_task->tf.sp);
    trap_return(&current_task->tf);
    // Never returns - signal delivered and process terminated
    debugk("ERROR - trap_return returned!\n");
  } else {
    debugk("SPP=0 in saved tf, returning normally\n");
  }
}

int handle_page_fault(uint64_t fault_addr, uint64_t scause, struct trap_frame *tf) {
  printk("[PF] fault_addr=0x%llx scause=%lld pid=%llu SPP=%d\n",
         fault_addr, scause, current_task->pid, !!(tf->sstatus & SSTATUS_SPP));
  debugk("=== PAGE FAULT START ===\n");
  debugk("Fault addr=0x%llx, scause=%lld, pid=%llu, SPP=%d\n",
         fault_addr, scause, current_task->pid, !!(tf->sstatus & SSTATUS_SPP));

  // Check if this is a kernel page fault (supervisor mode accessing kernel memory)
  // Allow supervisor mode to fault on user memory (for syscalls accessing user buffers)
  if ((tf->sstatus & SSTATUS_SPP) && fault_addr >= END_USER_SPACE_ADDR) {
    // Check if SP caused the fault (likely stack overflow into guard page)
    if (fault_addr >= tf->sp - 256 && fault_addr <= tf->sp + 256) {
      printk("KERNEL STACK OVERFLOW!\n");
      printk("  Stack pointer accessed unmapped memory\n");
      printk("  Fault address: 0x%llx\n", fault_addr);
      printk("  SP: 0x%llx\n", tf->sp);
      printk("  Kernel stack base: 0x%llx\n", KERNEL_STACK_VIRTUAL_BASE);
      printk("  Kernel stack top: 0x%llx\n", KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE);
      printk("  PC (sepc): 0x%llx\n", tf->sepc);
      printk("  RA: 0x%llx\n", tf->ra);
      if (current_task) {
        printk("  PID: %llu\n", current_task->pid);
      }
      panic("Kernel stack overflowed into unmapped guard page");
    }

    printk("KERNEL PAGE FAULT DETAILS:\n");
    printk("  Fault address: 0x%llx\n", fault_addr);
    printk("  Scause: %lld\n", scause);
    printk("  PC (sepc): 0x%llx\n", tf->sepc);
    printk("  SP: 0x%llx\n", tf->sp);
    printk("  RA: 0x%llx\n", tf->ra);
    printk("  Current task: %p\n", current_task);
    if (current_task) {
      printk("  PID: %llu\n", current_task->pid);
    }
    panic("Kernel page fault at 0x%llx", fault_addr);
  }

  struct vma_t *vma = find_vma(&current_task->mm_struct, fault_addr);

  if (!vma) {
    debugk("No VMA found for fault address 0x%llx, sending SIGSEGV\n", fault_addr);
    send_sigsegv_and_abort_syscall();
    return -1;
  }

  if (scause == 12 && !(vma->vm_flags & VM_EXEC)) {
    debugk("Instruction fetch from non-executable page, sending SIGSEGV\n");
    send_sigsegv_and_abort_syscall();
    return -1;
  }
  if (scause == 13 && !(vma->vm_flags & VM_READ)) {
    debugk("Load from non-readable page, sending SIGSEGV\n");
    send_sigsegv_and_abort_syscall();
    return -1;
  }
  if (scause == 15 && !(vma->vm_flags & VM_WRITE)) {
    debugk("Store to non-writable page, sending SIGSEGV\n");
    send_sigsegv_and_abort_syscall();
    return -1;
  }

  uint64_t pte = get_pte(current_task->mm_struct.root_satp, fault_addr);
  if (pte & PTE_VALID) {
    debugk("Page already mapped but permission fault, sending SIGSEGV\n");
    send_sigsegv_and_abort_syscall();
    return -1;
  }

  void *phys_page;
  if (vma->backing_file) {
    size_t offset = vma->offset + (PAGE_ALIGN_DOWN(fault_addr) - vma->start_addr);
    debugk("File-backed page fault, offset=0x%llx\n", offset);
    phys_page = vfs_get_page(vma->backing_file, offset, VFS_PAGE_REF);
  } else {
    debugk("Anonymous page fault\n");
    phys_page = get_zero_page(false);
  }

  uint64_t pte_flags = vma_to_pte_flags(vma->vm_flags);
  map_page(current_task->mm_struct.root_satp, PAGE_ALIGN_DOWN(fault_addr),
           (uint64_t)phys_page, pte_flags);

  debugk("Page fault handled successfully, mapped 0x%llx -> 0x%llx\n",
         PAGE_ALIGN_DOWN(fault_addr), (uint64_t)phys_page);
  return 0;
}
