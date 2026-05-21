#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "kernel/memory/page_tables.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/filesystem/vfs/vfs.h"

DEFINE_SYSCALL2(munmap, size_t, addr, size_t, len)
{
  if (len == 0) return 0;

  len = (len + DEFAULT_PAGE_SIZE - 1) & ~(DEFAULT_PAGE_SIZE - 1);
  size_t unmap_end = addr + len;

  struct list_node *pos = current_task->mm_struct.vma_list.next;
  while (pos != &current_task->mm_struct.vma_list) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    struct list_node *next = pos->next;

    // Check if this VMA overlaps [addr, unmap_end)
    if (vma->end_addr <= addr || vma->start_addr >= unmap_end) {
      pos = next;
      continue;
    }

    if (vma->backing_file == NULL) {
      for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
        uint64_t pte = get_pte(current_task->mm_struct.root_satp, va);
        if (pte & PTE_VALID) {
          void *phys_page = (void *)PTE_DECODE(pte);
          page_decref(phys_page);
        }
      }
      unmap_pages(current_task->mm_struct.root_satp, vma->start_addr, vma->end_addr);
    } else {
      vfs_address_space_drop_ref(vma->start_addr, vma->end_addr,
                                 vma->offset, vma->backing_file->address_space);
      unmap_pages(current_task->mm_struct.root_satp, vma->start_addr, vma->end_addr);
      vma->backing_file->refcount--;
    }

    list_remove(&vma->sibling_vma);
    vma_t_free(vma);
    pos = next;
  }

  asm volatile("sfence.vma zero, zero");
  return 0;
}
