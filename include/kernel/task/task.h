#ifndef KERNEL_TASK
#define KERNEL_TASK

#include "trap.h"
#include "lib/pool_allocator.h"
#include "kernel/memory/page_tables.h"
#include "lib/list.h"

// VMA flags
#define VM_READ     0x0001
#define VM_WRITE    0x0002
#define VM_EXEC     0x0004
#define VM_SHARED   0x0008

struct vma_t {
  size_t start_addr; // Virtual address Start
  size_t end_addr; // Virtual address end (can span multiple pages)
  uint64_t vm_flags; // VM_READ, VM_WRITE, VM_EXEC, etc.
  struct list_node sibling_vma;
  struct vnode_t *backing_file; // NULL if anonymous
  size_t offset; // File offset (in bytes, page-aligned)
};

struct mm_struct_t {
  page_table_t *root_satp;
  struct list_node vma_list;
};

struct task_t {
  struct trap_frame tf;
  void *kernel_stack;
  uint64_t pid;
  uint32_t uid;
  struct mm_struct_t mm_struct;
  struct task_t *next_task;
};

DEFINE_POOL(task_t, struct task_t)
DEFINE_POOL(vma_t, struct vma_t)

struct vma_t *find_vma(struct mm_struct_t *mm_struct, size_t vaddr);

int64_t file_backed_memory_map(struct mm_struct_t *mm_struct, size_t vaddr,
                                struct vnode_t *vnode, size_t offset,
                                size_t size, uint64_t vm_flags, bool eager);

#endif
