#include "arch/riscv64/syscalls/syscalls.h"
#include "arch/riscv64/syscalls/syscall_macros.h"
#include "kernel/task/task.h"
#include "kernel/memory/page_tables.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_ANONYMOUS 32

DEFINE_SYSCALL6(mmap,
    size_t, addr, size_t, len, int, prot, int, flags, int, fd, size_t, offset)
{
  if (len == 0) return -1;

  len = (len + DEFAULT_PAGE_SIZE - 1) & ~(DEFAULT_PAGE_SIZE - 1);

  uint64_t vm_flags = 0;
  if (prot & PROT_READ)  vm_flags |= VM_READ;
  if (prot & PROT_WRITE) vm_flags |= VM_WRITE;
  if (prot & PROT_EXEC)  vm_flags |= VM_EXEC;

  if (addr == 0)
    addr = find_free_vma(&current_task->mm_struct, len);

  if (flags & MAP_ANONYMOUS) {
    int64_t ret = anon_memory_map(&current_task->mm_struct, addr, len, vm_flags, false);
    if (ret < 0) return -1;
    return (int64_t)addr;
  } else {
    struct file_t *file = find_file(&current_task->file_table, fd);
    if (!file || !file->vnode) return -1;
    int64_t ret = file_backed_memory_map(&current_task->mm_struct, addr,
                                         file->vnode, offset, len, vm_flags, false);
    if (ret < 0) return -1;
    return (int64_t)addr;
  }
}
