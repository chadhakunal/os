#include "arch/riscv64/syscalls/syscall_macros.h"
#include "arch/riscv64/syscalls/syscalls.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "errno.h"
#include "types.h"

#define MS_ASYNC      1
#define MS_SYNC       4
#define MS_INVALIDATE 2

DEFINE_SYSCALL3(msync, void *, addr, size_t, len, int, flags)
{
  if (len == 0) return 0;
  if (flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) return -EINVAL;
  if ((flags & MS_ASYNC) && (flags & MS_SYNC)) return -EINVAL;

  size_t vaddr = (size_t)addr;
  size_t end   = vaddr + len;

  /* Walk VMAs that overlap [vaddr, end) and flush each file-backed one. */
  list_for_each(&current_task->mm_struct.vma_list, pos) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);

    if (vma->end_addr <= vaddr || vma->start_addr >= end)
      continue;
    if (vma->backing_file == NULL)
      continue;

    if (flags & MS_INVALIDATE)
      vfs_invalidate_page_cache(vma->backing_file);
    else
      vfs_flush_dirty_pages(vma->backing_file);
  }

  return 0;
}
