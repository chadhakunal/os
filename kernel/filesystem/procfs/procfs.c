#include "kernel/filesystem/procfs/procfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "kernel/time/timer.h"
#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/task/task.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

/* -------------------------------------------------------------------------
 * Buffer formatting helpers
 * ---------------------------------------------------------------------- */

static size_t buf_puts(char *buf, size_t cap, size_t pos, const char *s) {
  while (*s && pos < cap - 1)
    buf[pos++] = *s++;
  buf[pos] = '\0';
  return pos;
}

static size_t buf_putu64(char *buf, size_t cap, size_t pos, uint64_t n) {
  char tmp[20];
  int i = 0;
  if (n == 0) {
    tmp[i++] = '0';
  } else {
    while (n > 0) {
      tmp[i++] = '0' + (n % 10);
      n /= 10;
    }
  }
  while (i > 0 && pos < cap - 1)
    buf[pos++] = tmp[--i];
  buf[pos] = '\0';
  return pos;
}

static int64_t copy_slice(void *dst, uint64_t size,
                           const char *src, size_t src_len,
                           uint64_t offset) {
  if (offset >= src_len)
    return 0;
  size_t available = src_len - offset;
  size_t n = (size < available) ? size : available;
  memcpy(dst, src + offset, n);
  return (int64_t)n;
}

/* -------------------------------------------------------------------------
 * /proc/uptime
 * ---------------------------------------------------------------------- */
static int64_t proc_uptime_read(struct file_t *file, uint64_t offset,
                                void *buf, uint64_t size) {
  (void)file;
  char tmp[128];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, "ticks: ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.os_ticks);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\ncycles: ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.system_uptime);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_uptime_fops;

/* -------------------------------------------------------------------------
 * /proc/meminfo
 * ---------------------------------------------------------------------- */
static int64_t proc_meminfo_read(struct file_t *file, uint64_t offset,
                                 void *buf, uint64_t size) {
  (void)file;
  uint64_t total_kb = (pages_metadata.total_pages * DEFAULT_PAGE_SIZE) / 1024;
  uint64_t used_kb  = (pages_metadata.pages_in_use * DEFAULT_PAGE_SIZE) / 1024;
  uint64_t free_kb  = total_kb - used_kb;

  char tmp[256];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, "MemTotal:      ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, total_kb);
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nMemFree:       ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, free_kb);
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nMemUsed:       ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, used_kb);
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_meminfo_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/status
 * ---------------------------------------------------------------------- */
static const char *task_state_str(enum task_state state) {
  switch (state) {
    case TASK_RUNNING:    return "R (running)";
    case TASK_READY:      return "R (ready)";
    case TASK_BLOCKED:    return "S (sleeping)";
    case TASK_ZOMBIE:     return "Z (zombie)";
    case TASK_TERMINATED: return "X (dead)";
    default:              return "? (unknown)";
  }
}

static int64_t proc_pid_status_read(struct file_t *file, uint64_t offset,
                                    void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  char tmp[256];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, "Pid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nPPid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->ppid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nState:\t");
  pos = buf_puts(tmp, sizeof(tmp), pos, task_state_str(task->state));
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nUid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->uid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_pid_status_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/ directory ops
 * ---------------------------------------------------------------------- */
static struct vnode_ops_t proc_pid_dir_ops;

static int64_t procfs_pid_readdir(struct vnode_t *dir, uint32_t index,
                                  struct dentry_t **out) {
  if (index == 0) {
    struct vnode_t *vnode = vnode_t_alloc();
    vfs_init_vnode(vnode, dir->superblock, 0);
    vnode->permission_mode = S_IFREG | READ_EXECUTE_PERM;
    vnode->file_ops        = &proc_pid_status_fops;
    vnode->fs_private_vnode = dir->fs_private_vnode;

    struct dentry_t *d = dentry_t_alloc();
    strncpy(d->name, "status", sizeof(d->name) - 1);
    d->vnode  = vnode;
    d->parent = NULL;
    *out = d;
    return 0;
  }
  *out = NULL;
  return -1;
}

static int64_t procfs_pid_lookup(const char *name, struct vnode_t *parent,
                                 struct dentry_t **out) {
  if (strncmp(name, "status") == 0) {
    struct vnode_t *vnode = vnode_t_alloc();
    vfs_init_vnode(vnode, parent->superblock, 0);
    vnode->permission_mode  = S_IFREG | READ_EXECUTE_PERM;
    vnode->file_ops         = &proc_pid_status_fops;
    vnode->fs_private_vnode = parent->fs_private_vnode;

    struct dentry_t *d = dentry_t_alloc();
    strncpy(d->name, "status", sizeof(d->name) - 1);
    d->vnode  = vnode;
    d->parent = NULL;
    *out = d;
    return 0;
  }
  *out = NULL;
  return -ENOENT;
}

/* -------------------------------------------------------------------------
 * Helper — build a PID directory dentry+vnode on demand.
 * ---------------------------------------------------------------------- */
static struct dentry_t *proc_make_pid_dentry(struct superblock_t *sb,
                                              uint64_t pid) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, 0);
  vnode->permission_mode  = S_IFDIR | READ_EXECUTE_PERM;
  vnode->vnode_ops        = &proc_pid_dir_ops;
  vnode->fs_private_vnode = (void *)pid;

  /* Convert pid to decimal string for the dentry name. */
  char pid_str[20];
  int i = 0;
  char rev[20];
  int j = 0;
  uint64_t tmp = pid;
  if (tmp == 0) {
    rev[j++] = '0';
  } else {
    while (tmp > 0) { rev[j++] = '0' + (tmp % 10); tmp /= 10; }
  }
  while (j > 0) pid_str[i++] = rev[--j];
  pid_str[i] = '\0';

  struct dentry_t *d = dentry_t_alloc();
  strncpy(d->name, pid_str, sizeof(d->name) - 1);
  d->vnode  = vnode;
  d->parent = NULL;
  return d;
}

/* -------------------------------------------------------------------------
 * /proc root readdir — static files, then one entry per live task.
 * ---------------------------------------------------------------------- */
static int64_t procfs_root_readdir(struct vnode_t *dir, uint32_t index,
                                   struct dentry_t **out) {
  uint32_t i = 0;

  /* Static children first (uptime, etc.) */
  list_for_each(&dir->children_dentries, pos) {
    if (i == index) {
      *out = container_of(pos, struct dentry_t, sibling_dentry);
      return 0;
    }
    i++;
  }

  /* Dynamic PID directories from the global task list. */
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (i == index) {
      *out = proc_make_pid_dentry(dir->superblock, task->pid);
      return 0;
    }
    i++;
  }

  *out = NULL;
  return -1;
}

/* -------------------------------------------------------------------------
 * /proc root lookup
 * ---------------------------------------------------------------------- */
static int64_t procfs_root_lookup(const char *name, struct vnode_t *parent,
                                  struct dentry_t **out) {
  /* Static children first. */
  list_for_each(&parent->children_dentries, pos) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(d->name, name) == 0) {
      *out = d;
      return 0;
    }
  }

  /* Try parsing the name as a decimal PID. */
  uint64_t pid = 0;
  for (const char *p = name; *p; p++) {
    if (*p < '0' || *p > '9') {
      *out = NULL;
      return -ENOENT;
    }
    pid = pid * 10 + (uint64_t)(*p - '0');
  }

  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL) {
    *out = NULL;
    return -ENOENT;
  }

  *out = proc_make_pid_dentry(parent->superblock, pid);
  return 0;
}

/* -------------------------------------------------------------------------
 * Helper — attach a static read-only file to a parent vnode.
 * ---------------------------------------------------------------------- */
static void proc_add_file(struct superblock_t *sb, struct vnode_t *parent,
                           uint32_t *id, const char *name,
                           struct file_ops_t *fops) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, (*id)++);
  vnode->permission_mode = S_IFREG | READ_EXECUTE_PERM;
  vnode->file_ops        = fops;

  struct dentry_t *dentry = dentry_t_alloc();
  strncpy(dentry->name, name, sizeof(dentry->name) - 1);
  dentry->vnode  = vnode;
  dentry->parent = NULL;
  list_append(&parent->children_dentries, &dentry->sibling_dentry);
}

/* -------------------------------------------------------------------------
 * Mount
 * ---------------------------------------------------------------------- */
struct superblock_t *procfs_mount(void) {
  /* Assign all function pointers at runtime to get virtual addresses. */
  proc_uptime_fops.read       = proc_uptime_read;
  proc_meminfo_fops.read      = proc_meminfo_read;
  proc_pid_status_fops.read   = proc_pid_status_read;
  proc_pid_dir_ops.readdir    = procfs_pid_readdir;
  proc_pid_dir_ops.lookup     = procfs_pid_lookup;

  struct superblock_t *sb = superblock_t_alloc();
  sb->flags             = SB_NODENTRY_CACHE;
  sb->vnode_ops.readdir = procfs_root_readdir;
  sb->vnode_ops.lookup  = procfs_root_lookup;

  struct vnode_t *root = vnode_t_alloc();
  uint32_t id = 0;
  vfs_init_vnode(root, sb, id++);
  root->permission_mode = S_IFDIR | READ_EXECUTE_PERM;
  sb->root_vnode = root;

  proc_add_file(sb, root, &id, "uptime",  &proc_uptime_fops);
  proc_add_file(sb, root, &id, "meminfo", &proc_meminfo_fops);

  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "proc", sizeof(root_dentry->name) - 1);
  root_dentry->vnode  = root;
  root_dentry->parent = NULL;
  sb->root_dentry = root_dentry;

  return sb;
}
