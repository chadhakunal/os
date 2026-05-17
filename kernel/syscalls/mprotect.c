#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "kernel/memory/page_tables.h"
#include "errno.h"

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

static uint64_t prot_to_vm_flags(int prot) {
  uint64_t vm_flags = 0;

  if (prot & PROT_READ)
    vm_flags |= VM_READ;
  if (prot & PROT_WRITE)
    vm_flags |= VM_WRITE;
  if (prot & PROT_EXEC)
    vm_flags |= VM_EXEC;
  return vm_flags;
}

static uint64_t vm_flags_to_pte_perm(uint64_t vm_flags) {
  uint64_t pte_perm = PTE_U;

  if (vm_flags & VM_READ)
    pte_perm |= PTE_R;
  if (vm_flags & VM_WRITE)
    pte_perm |= PTE_W | PTE_D;
  if (vm_flags & VM_EXEC)
    pte_perm |= PTE_X;
  return pte_perm;
}

DEFINE_SYSCALL3(mprotect, size_t, addr, size_t, len, int, prot)
{
  if (len == 0)
    return 0;

  if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
    return -EINVAL;

  size_t start = PAGE_ALIGN_DOWN(addr);
  size_t end = PAGE_ALIGN_UP(addr + len);
  if (end <= start)
    return 0;

  uint64_t vm_flags = prot_to_vm_flags(prot);
  struct mm_struct_t *mm = &current_task->mm_struct;
  struct vma_t *range_vma = NULL;

  for (size_t va = start; va < end; va += DEFAULT_PAGE_SIZE) {
    struct vma_t *vma = find_vma(mm, va);
    if (!vma)
      return -ENOMEM;
    if (!range_vma)
      range_vma = vma;
    else if (vma != range_vma)
      return -ENOMEM;
  }

  if (start < range_vma->start_addr || end > range_vma->end_addr)
    return -ENOMEM;

  range_vma->vm_flags = vm_flags;

  if (!vm_flags) {
    for (size_t va = start; va < end; va += DEFAULT_PAGE_SIZE) {
      if (get_pte(mm->root_satp, va) & PTE_VALID)
        unmap_page(mm->root_satp, va);
    }
    asm volatile("sfence.vma zero, zero" ::: "memory");
    return 0;
  }

  uint64_t pte_perm = vm_flags_to_pte_perm(vm_flags);
  for (size_t va = start; va < end; va += DEFAULT_PAGE_SIZE) {
    uint64_t pte = get_pte(mm->root_satp, va);
    if (pte & PTE_VALID) {
      uint64_t pa = PTE_DECODE(pte);
      map_page(mm->root_satp, va, pa, pte_perm);
      asm volatile("sfence.vma %0, zero" ::"r"(va) : "memory");
    }
  }

  return 0;
}
