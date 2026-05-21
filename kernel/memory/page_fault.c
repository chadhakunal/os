#define DEBUG 0
#include "kernel/memory/page_fault.h"
#include "kernel/task/task.h"
#include "kernel/task/signal.h"
#include "kernel/memory/page_tables.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/trap.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"
#include "kernel/panic.h"
#include "lib/string.h"


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
  extern void kernel_fault_recovery_jump(struct kernel_fault_recovery_t *r);

  debugk("Sending SIGSEGV to PID %llu\n", current_task->pid);

  // If inside copy_to/from_user, jump back so it returns -1 to the syscall.
  // Don't send SIGSEGV — the syscall will propagate the error to userspace.
  if (current_task->fault_recovery.active) {
    debugk("Kernel fault recovery active, jumping back to copy_to/from_user\n");
    current_task->fault_recovery.active = 0;
    kernel_fault_recovery_jump(&current_task->fault_recovery);
    // never returns
  }

  // Direct user-mode fault — send SIGSEGV and return to userspace for delivery.
  send_signal(current_task, SIGSEGV);
  debugk("Returning to user PC=0x%llx SP=0x%llx\n",
         current_task->tf.sepc, current_task->tf.sp);
  trap_return(&current_task->tf);
}

static void handle_cow_fault(uint64_t fault_addr, uint64_t faulting_pte) {
  uint64_t aligned = PAGE_ALIGN_DOWN(fault_addr);
  void *old_phys = (void *)PTE_DECODE(faulting_pte);

  void *new_phys;
  if (page_get_refcount(old_phys) == 1) {
    // Last owner — reclaim in place, no copy needed
    new_phys = old_phys;
  } else {
    // Shared — allocate a private copy
    new_phys = get_page(false);
    if (!new_phys)
      oom_kill_current();
    memcpy(PHYS_TO_VIRT(new_phys), PHYS_TO_VIRT(old_phys), DEFAULT_PAGE_SIZE);
    page_decref(old_phys); // Release our share of the old page
  }

  // Restore PTE_W, clear PTE_COW, keep other permission flags
  uint64_t new_pte = PTE_ADDR(new_phys) | PTE_VALID |
                     (faulting_pte & (PTE_R | PTE_X | PTE_U | PTE_A)) |
                     PTE_W | PTE_D;
  set_pte(current_task->mm_struct.root_satp, aligned, new_pte);

  asm volatile("sfence.vma %0, zero" :: "r"(aligned) : "memory");

  debugk("cow_fault: pid=%llu va=%llx old_phys=%p new_phys=%p\n",
         current_task->pid, aligned, old_phys, new_phys);
}

int handle_page_fault(uint64_t fault_addr, uint64_t scause, struct trap_frame *tf) {
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
    if (scause == 15 && (pte & PTE_COW)) {
      // Store fault on a COW page — break the sharing
      handle_cow_fault(fault_addr, pte);
      return 0;
    }
    debugk("Page already mapped but permission fault, sending SIGSEGV\n");
    send_sigsegv_and_abort_syscall();
    return -1;
  }

  void *phys_page;
  if (vma->backing_file) {
    size_t offset = vma->offset + (PAGE_ALIGN_DOWN(fault_addr) - vma->start_addr);
    phys_page = vfs_get_page(vma->backing_file, offset, VFS_PAGE_REF);
  } else {
    phys_page = get_zero_page(false);
    if (!phys_page) {
      phys_page = get_page(false);
      if (phys_page)
        memset(PHYS_TO_VIRT(phys_page), 0, DEFAULT_PAGE_SIZE);
    }
  }

  if (!phys_page) {
    oom_kill_current();
    return -1;
  }

  uint64_t pte_flags = vma_to_pte_flags(vma->vm_flags);
  map_page(current_task->mm_struct.root_satp, PAGE_ALIGN_DOWN(fault_addr),
           (uint64_t)phys_page, pte_flags);
  asm volatile("sfence.vma %0, zero" :: "r"(PAGE_ALIGN_DOWN(fault_addr)) : "memory");

  debugk("Page fault handled successfully, mapped 0x%llx -> 0x%llx\n",
         PAGE_ALIGN_DOWN(fault_addr), (uint64_t)phys_page);
  return 0;
}
