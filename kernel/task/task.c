#include "kernel/task/task.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "virtual_memory_init.h"
#include "panic.h"

struct task_t *init_task() {
  struct task_t *task = task_t_alloc();
  task->kernel_stack = PHYS_TO_VIRT(get_page(true));
  task->pid = 0;
  task->uid = 0;
  // Initialize page table with kernel mappings copied from root
  task->mm_struct.root_satp = init_new_page_table();
  // Initialize VMA list
  task->mm_struct.vma_list.next = &task->mm_struct.vma_list;
  task->mm_struct.vma_list.prev = &task->mm_struct.vma_list;
  return task;
}

struct task_t *create_init_process() {
  struct task_t *init_task = task_t_alloc();
  return init_task;
}

struct vma_t *find_vma(struct mm_struct_t *mm_struct, size_t vaddr) {
  size_t vaddr_aligned = vaddr & ~(DEFAULT_PAGE_SIZE - 1);
  list_for_each(&mm_struct->vma_list, pos) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    // Check if vaddr is within this VMA's range
    if (vaddr_aligned >= vma->start_addr && vaddr_aligned < vma->end_addr) {
      return vma;
    }
  }
  return NULL;
}

int64_t file_backed_memory_map(struct mm_struct_t *mm_struct, size_t vaddr,
                                struct vnode_t *vnode, size_t offset,
                                size_t size, uint64_t vm_flags, bool eager) {
  if (mm_struct == NULL || vnode == NULL || size == 0) {
    panic("file_backed_memory_map: invalid parameters\n");
  }

  printk("file_backed_memory_map: vaddr=0x%lx, offset=0x%lx, size=0x%lx, vm_flags=0x%lx, eager=%d\n",
         vaddr, offset, size, vm_flags, eager);

  size_t vaddr_aligned = vaddr & ~(DEFAULT_PAGE_SIZE - 1);
  size_t offset_aligned = offset & ~(DEFAULT_PAGE_SIZE - 1);
  size_t offset_in_page = vaddr - vaddr_aligned;
  size_t total_size = offset_in_page + size;
  size_t num_pages = (total_size + DEFAULT_PAGE_SIZE - 1) / DEFAULT_PAGE_SIZE;
  size_t vaddr_end = vaddr_aligned + (num_pages * DEFAULT_PAGE_SIZE);

  printk("  Aligned: vaddr=0x%lx, offset=0x%lx, num_pages=%lu, vaddr_end=0x%lx\n",
         vaddr_aligned, offset_aligned, num_pages, vaddr_end);
 
  for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    struct vma_t *existing = find_vma(mm_struct, va);
    if (existing != NULL) {
      printk("  ERROR: VMA already exists at 0x%lx (range 0x%lx-0x%lx)\n",
             va, existing->start_addr, existing->end_addr);
      return -1;
    }
  }

  struct vma_t *new_vma = vma_t_alloc();
  new_vma->start_addr = vaddr_aligned;
  new_vma->end_addr = vaddr_end;
  new_vma->backing_file = vnode;
  new_vma->offset = offset_aligned;
  new_vma->vm_flags = vm_flags;

  list_append(&mm_struct->vma_list, &new_vma->sibling_vma);
  printk("  Created VMA: 0x%lx-0x%lx, file_offset=0x%lx, flags=0x%lx\n",
         new_vma->start_addr, new_vma->end_addr, new_vma->offset, new_vma->vm_flags);

  // Eagerly load the pages into the page table from the file
  if (eager) {
    printk("  Eagerly loading %lu pages...\n", num_pages);
    size_t file_offset = offset_aligned;
    for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
      void *phys_page = vfs_get_page(vnode, file_offset);

      // Convert VM flags to PTE flags
      // Note: map_page() sets PTE_VALID and PTE_A automatically
      uint64_t pte_flags = PTE_U;
      if (vm_flags & VM_READ)  pte_flags |= PTE_R;
      if (vm_flags & VM_WRITE) pte_flags |= PTE_W;
      if (vm_flags & VM_EXEC)  pte_flags |= PTE_X;

      map_page(mm_struct->root_satp, va, (uint64_t)phys_page, pte_flags);
      printk("    Mapped page: va=0x%lx -> pa=0x%lx, file_offset=0x%lx, pte_flags=0x%lx\n",
             va, (uint64_t)phys_page, file_offset, pte_flags);
      file_offset += DEFAULT_PAGE_SIZE;
    }
  }

  printk("  file_backed_memory_map: SUCCESS\n");
  return 0;
}
