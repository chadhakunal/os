#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/pipefs/pipe.h"
#include "kernel/task/task.h"
#include "kernel/resource.h"
#include "kernel/memory/page_allocator.h"
#include "lib/list.h"
#include "errno.h"

// lseek whence values
#define SEEK_SET 0  // Seek from beginning of file
#define SEEK_CUR 1  // Seek from current position
#define SEEK_END 2  // Seek from end of file

static struct file_t *find_file_by_fd(struct files_table_t *file_table, int fd, struct files_list_t **out_list, int *out_local_fd) {
  if (fd < 0) {
    return NULL;
  }

  int base_fd = 0;

  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *files_list = container_of(pos, struct files_list_t, files_list);

    if (fd >= base_fd && fd < base_fd + 32) {
      int local_fd = fd - base_fd;

      // Check if fd is allocated
      if (!(files_list->used_file_bitmap & (1 << local_fd))) {
        return NULL;
      }

      struct file_t *file = files_list->files[local_fd];
      if (file == NULL) {
        return NULL;
      }

      // Return optional outputs
      if (out_list) {
        *out_list = files_list;
      }
      if (out_local_fd) {
        *out_local_fd = local_fd;
      }

      return file;
    }
    base_fd += 32;
  }

  return NULL;
}

int64_t vfs_file_lseek(struct files_table_t *file_table, int fd, int64_t offset, int whence) {
  struct file_t *file = find_file_by_fd(file_table, fd, NULL, NULL);
  if (file == NULL) {
    return -EBADF;
  }

  if (file->pipe != NULL)
    return -ESPIPE;

  int64_t new_offset;

  switch (whence) {
    case SEEK_SET:
      new_offset = offset;
      break;

    case SEEK_CUR:
      new_offset = (int64_t)file->offset + offset;
      break;

    case SEEK_END:
      if (file->vnode == NULL) {
        return -EBADF;
      }
      new_offset = (int64_t)file->vnode->size + offset;
      break;

    default:
      return -EINVAL;
  }

  // Check for negative offset
  if (new_offset < 0) {
    return -EINVAL;
  }

  file->offset = (size_t)new_offset;

  return new_offset;
}

static int find_free_fd_ge(struct files_table_t *file_table, int min_fd) {
  int base_fd = 0;

  if (min_fd < 0)
    return -EINVAL;

  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *files_list =
        container_of(pos, struct files_list_t, files_list);

    for (int local_fd = 0; local_fd < 32; local_fd++) {
      int fd = base_fd + local_fd;

      if (fd < min_fd)
        continue;

      if (!(files_list->used_file_bitmap & (1U << local_fd)))
        return fd;
    }
    base_fd += 32;
  }

  if (current_task != NULL) {
    int rlimit_ret = rlimit_check_nofile(current_task, 1);
    if (rlimit_ret < 0)
      return rlimit_ret;
  }

  struct files_list_t *files_list = files_list_t_alloc();
  if (files_list == NULL)
    return -EMFILE;

  files_list->used_file_bitmap = 0;
  files_list->close_on_exec_bitmap = 0;
  list_append(&file_table->files_list, &files_list->files_list);

  if (min_fd >= base_fd + 32)
    return -EMFILE;

  return min_fd > base_fd ? min_fd : base_fd;
}

int64_t vfs_fcntl_dup(struct files_table_t *file_table, int fd, int min_fd) {
  if (find_file_by_fd(file_table, fd, NULL, NULL) == NULL)
    return -EBADF;

  int newfd = find_free_fd_ge(file_table, min_fd);
  if (newfd < 0)
    return newfd;

  return vfs_dup2(file_table, fd, newfd);
}

int64_t vfs_dup2(struct files_table_t *file_table, int oldfd, int newfd) {
  if (oldfd == newfd)
    return newfd;

  /* Find the source file. */
  struct file_t *src = find_file_by_fd(file_table, oldfd, NULL, NULL);
  if (src == NULL)
    return -EBADF;

  bool newfd_was_open = find_file_by_fd(file_table, newfd, NULL, NULL) != NULL;

  /* Close newfd if it is already open. */
  vfs_file_close(file_table, newfd);

  if (!newfd_was_open && current_task != NULL) {
    int rlimit_ret = rlimit_check_nofile(current_task, 1);
    if (rlimit_ret < 0)
      return rlimit_ret;
  }

  /* Walk to the files_list node that owns newfd, allocating a new node if
   * the fd falls beyond the current highest node. */
  int base_fd = 0;
  struct files_list_t *target = NULL;

  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *fl = container_of(pos, struct files_list_t, files_list);
    if (newfd >= base_fd && newfd < base_fd + 32) {
      target = fl;
      break;
    }
    base_fd += 32;
  }

  if (target == NULL) {
    /* newfd is beyond existing nodes — allocate one. */
    target = files_list_t_alloc();
    if (target == NULL)
      return -ENOMEM;
    target->used_file_bitmap = 0;
    target->close_on_exec_bitmap = 0;
    list_append(&file_table->files_list, &target->files_list);
    /* base_fd already points to the start of this new node. */
  }

  int local_fd = newfd - base_fd;
  target->used_file_bitmap |= (1 << local_fd);
  target->close_on_exec_bitmap &= ~(1U << local_fd);

  if (src->pipe != NULL) {
    /* Give the new fd its own file_t so each close independently adjusts
     * the pipe's writer/reader count — same approach as copy_file_table. */
    struct file_t *dup_file = file_t_alloc();
    dup_file->vnode          = NULL;
    dup_file->dentry         = NULL;
    dup_file->file_ops       = NULL;
    dup_file->offset         = 0;
    dup_file->refcount       = 1;
    dup_file->flags          = src->flags;
    dup_file->pipe           = src->pipe;
    dup_file->pipe_write_end = src->pipe_write_end;
    if (src->pipe_write_end)
      src->pipe->writer_count++;
    else
      src->pipe->reader_count++;
    target->files[local_fd] = dup_file;
  } else {
    target->files[local_fd] = src;
    src->refcount++;
  }

  return newfd;
}

int vfs_file_get_fd_flags(struct files_table_t *file_table, int fd) {
  struct files_list_t *fl;
  int local_fd;

  if (find_file_by_fd(file_table, fd, &fl, &local_fd) == NULL)
    return -EBADF;

  if (fl->close_on_exec_bitmap & (1U << local_fd))
    return FD_CLOEXEC;
  return 0;
}

int64_t vfs_file_set_fd_flags(struct files_table_t *file_table, int fd, int flags) {
  struct files_list_t *fl;
  int local_fd;

  if (find_file_by_fd(file_table, fd, &fl, &local_fd) == NULL)
    return -EBADF;

  if (flags & FD_CLOEXEC)
    fl->close_on_exec_bitmap |= (1U << local_fd);
  else
    fl->close_on_exec_bitmap &= ~(1U << local_fd);

  return 0;
}

void vfs_file_set_close_on_exec(struct files_table_t *file_table, int fd) {
  struct files_list_t *fl;
  int local_fd;

  if (find_file_by_fd(file_table, fd, &fl, &local_fd) != NULL)
    fl->close_on_exec_bitmap |= (1U << local_fd);
}

void vfs_files_table_close_on_exec(struct files_table_t *file_table) {
  int base_fd = 0;

  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *fl = container_of(pos, struct files_list_t, files_list);
    uint32_t cloexec = fl->close_on_exec_bitmap & fl->used_file_bitmap;

    for (int local_fd = 0; local_fd < 32; local_fd++) {
      if (cloexec & (1U << local_fd))
        vfs_file_close(file_table, base_fd + local_fd);
    }
    base_fd += 32;
  }
}

static void vnode_drop_ref(struct vnode_t *vnode) {
  if (vnode->refcount == 0)
    return;
  vnode->refcount--;
  if (vnode->refcount > 0)
    return;

  /* Last reference gone — free all cached pages for this vnode. */
  if (vnode->address_space == NULL)
    return;

  struct address_space_ops_t *as_ops = vnode->address_space->address_space_ops;

  struct list_node *pos = vnode->address_space->page_cache_list.next;
  while (pos != &vnode->address_space->page_cache_list) {
    struct page_cache_entry_t *entry =
      container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    pos = pos->next;

    if (entry->dirty && as_ops != NULL && as_ops->write_page != NULL)
      as_ops->write_page(vnode, entry->offset, entry->physical_page);

    free_page(entry->physical_page);
    list_remove(&entry->sibling_page_cache_entry);
    page_cache_entry_t_free(entry);
  }
}

int64_t vfs_fsync(struct files_table_t *file_table, int fd) {
  struct file_t *file = find_file_by_fd(file_table, fd, NULL, NULL);
  if (file == NULL)
    return -EBADF;

  struct vnode_t *vnode = file->vnode;
  if (vnode->address_space == NULL)
    return 0;

  struct address_space_ops_t *as_ops = vnode->address_space->address_space_ops;

  list_for_each(&vnode->address_space->page_cache_list, pos) {
    struct page_cache_entry_t *entry =
      container_of(pos, struct page_cache_entry_t, sibling_page_cache_entry);
    if (entry->dirty && as_ops != NULL && as_ops->write_page != NULL) {
      as_ops->write_page(vnode, entry->offset, entry->physical_page);
      entry->dirty = false;
    }
  }

  return 0;
}

int64_t vfs_file_close(struct files_table_t *file_table, int fd) {
  struct files_list_t *files_list;
  int local_fd;

  struct file_t *file = find_file_by_fd(file_table, fd, &files_list, &local_fd);
  if (file == NULL) {
    return -EBADF;
  }

  file->refcount--;

  if (file->refcount == 0) {
    if (file->pipe != NULL) {
      pipe_close(file->pipe, file->pipe_write_end);
    } else {
      vnode_drop_ref(file->vnode);
    }
    file_t_free(file);
  }

  files_list->used_file_bitmap &= ~(1U << local_fd);
  files_list->close_on_exec_bitmap &= ~(1U << local_fd);
  files_list->files[local_fd] = NULL;

  return 0;
}
