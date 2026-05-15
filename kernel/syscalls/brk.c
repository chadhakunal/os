#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "kernel/memory/page_tables.h"
#include "kernel/memory/page_allocator.h"

DEFINE_SYSCALL1(brk, size_t, new_brk)
{
  size_t cur = current_task->mm_struct.heap_end;

  if (new_brk == 0)
    return (int64_t)cur;

  new_brk = (new_brk + DEFAULT_PAGE_SIZE - 1) & ~(DEFAULT_PAGE_SIZE - 1);

  if (new_brk > cur) {
    int64_t ret = anon_memory_map(&current_task->mm_struct, cur,
                                  new_brk - cur, VM_READ | VM_WRITE, false);
    if (ret < 0) return (int64_t)cur;
    current_task->mm_struct.heap_end = new_brk;
  } else if (new_brk < cur) {
    struct list_node *pos = current_task->mm_struct.vma_list.next;
    while (pos != &current_task->mm_struct.vma_list) {
      struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
      struct list_node *next = pos->next;
      if (vma->backing_file == NULL &&
          vma->start_addr >= new_brk && vma->end_addr <= cur) {
        for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
          uint64_t pte = get_pte(current_task->mm_struct.root_satp, va);
          if (pte & PTE_VALID)
            free_page((void *)PTE_DECODE(pte));
        }
        unmap_pages(current_task->mm_struct.root_satp, vma->start_addr, vma->end_addr);
        list_remove(&vma->sibling_vma);
        vma_t_free(vma);
      }
      pos = next;
    }
    asm volatile("sfence.vma zero, zero");
    current_task->mm_struct.heap_end = new_brk;
  }

  return (int64_t)current_task->mm_struct.heap_end;
}
