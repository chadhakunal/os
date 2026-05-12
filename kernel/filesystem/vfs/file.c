#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/task/task.h"
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

int64_t vfs_dup2(struct files_table_t *file_table, int oldfd, int newfd) {
  if (oldfd == newfd)
    return newfd;

  /* Find the source file. */
  struct file_t *src = find_file_by_fd(file_table, oldfd, NULL, NULL);
  if (src == NULL)
    return -EBADF;

  /* Close newfd if it is already open. */
  vfs_file_close(file_table, newfd);

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
    list_append(&file_table->files_list, &target->files_list);
    /* base_fd already points to the start of this new node. */
  }

  int local_fd = newfd - base_fd;
  target->files[local_fd] = src;
  target->used_file_bitmap |= (1 << local_fd);
  src->refcount++;

  return newfd;
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
    file_t_free(file);
  }

  files_list->used_file_bitmap &= ~(1 << local_fd);
  files_list->files[local_fd] = NULL;

  return 0;
}
