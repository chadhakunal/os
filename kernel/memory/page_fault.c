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

#define STACK_LIMIT (32 * 1024)

static uint64_t vma_to_pte_flags(uint64_t vm_flags) {
  uint64_t pte_flags = PTE_VALID | PTE_A | PTE_U;

  if (vm_flags & VM_READ)  pte_flags |= PTE_R;
  if (vm_flags & VM_WRITE) pte_flags |= PTE_W | PTE_D;
  if (vm_flags & VM_EXEC)  pte_flags |= PTE_X;

  return pte_flags;
}

int handle_page_fault(uint64_t fault_addr, uint64_t scause, struct trap_frame *tf) {
  // Check if this is a kernel page fault (supervisor mode accessing kernel memory)
  // Allow supervisor mode to fault on user memory (for syscalls accessing user buffers)
  if ((tf->sstatus & SSTATUS_SPP) && fault_addr >= END_USER_SPACE_ADDR) {
    panic("Kernel page fault at 0x%llx", fault_addr);
  }

  debugk("Page fault at 0x%llx, scause=%lld, pid=%llu, SPP=%d\n",
         fault_addr, scause, current_task->pid, !!(tf->sstatus & SSTATUS_SPP));

  struct vma_t *vma = find_vma(&current_task->mm_struct, fault_addr);

  if (!vma) {
    uint64_t stack_limit = DEFAULT_STACK_START - STACK_LIMIT;

    if (fault_addr < DEFAULT_STACK_START && fault_addr >= stack_limit) {
      struct vma_t *stack_vma = find_vma(&current_task->mm_struct, DEFAULT_STACK_START - 1);

      if (stack_vma && fault_addr < stack_vma->start_addr) {
        uint64_t new_start = PAGE_ALIGN_DOWN(fault_addr);
        debugk("Growing stack from 0x%llx to 0x%llx\n",
               stack_vma->start_addr, new_start);
        stack_vma->start_addr = new_start;

        void *phys_page = get_zero_page(false);
        uint64_t pte_flags = PTE_R | PTE_W | PTE_U | PTE_VALID | PTE_A | PTE_D;
        map_page(current_task->mm_struct.root_satp, PAGE_ALIGN_DOWN(fault_addr),
                 (uint64_t)phys_page, pte_flags);

        debugk("Stack grown successfully, mapped page at 0x%llx\n",
               PAGE_ALIGN_DOWN(fault_addr));
        return 0;
      }
    }

    debugk("No VMA found for fault address 0x%llx, sending SIGSEGV\n", fault_addr);
    send_signal(current_task, SIGSEGV);
    return -1;
  }

  if (scause == 12 && !(vma->vm_flags & VM_EXEC)) {
    debugk("Instruction fetch from non-executable page, sending SIGSEGV\n");
    send_signal(current_task, SIGSEGV);
    return -1;
  }
  if (scause == 13 && !(vma->vm_flags & VM_READ)) {
    debugk("Load from non-readable page, sending SIGSEGV\n");
    send_signal(current_task, SIGSEGV);
    return -1;
  }
  if (scause == 15 && !(vma->vm_flags & VM_WRITE)) {
    debugk("Store to non-writable page, sending SIGSEGV\n");
    send_signal(current_task, SIGSEGV);
    return -1;
  }

  uint64_t pte = get_pte(current_task->mm_struct.root_satp, fault_addr);
  if (pte & PTE_VALID) {
    debugk("Page already mapped but permission fault, sending SIGSEGV\n");
    send_signal(current_task, SIGSEGV);
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
