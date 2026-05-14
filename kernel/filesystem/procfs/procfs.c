#include "kernel/filesystem/procfs/procfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "kernel/time/timer.h"
#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

/* -------------------------------------------------------------------------
 * Minimal buffer formatting helpers
 *
 * All helpers take:
 *   buf  - destination buffer
 *   cap  - total buffer capacity
 *   pos  - current write position (cursor)
 * They return the new cursor position after writing.
 * ---------------------------------------------------------------------- */

/* Append a string to buf. */
static size_t buf_puts(char *buf, size_t cap, size_t pos, const char *s) {
  while (*s && pos < cap - 1)
    buf[pos++] = *s++;
  buf[pos] = '\0';
  return pos;
}

/* Append an unsigned 64-bit number in decimal to buf. */
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
  /* tmp holds digits in reverse — write them forward into buf. */
  while (i > 0 && pos < cap - 1)
    buf[pos++] = tmp[--i];
  buf[pos] = '\0';
  return pos;
}

/* Copy the slice [offset, offset+size) of src (length src_len) into dst.
 * Returns the number of bytes actually copied. */
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
 *
 * Format:  "ticks: <os_ticks>\ncycles: <system_uptime>\n"
 * ---------------------------------------------------------------------- */
static int64_t proc_uptime_read(struct file_t *file, uint64_t offset,
                                void *buf, uint64_t size) {
  debugk("proc_uptime_read: entered, offset=%llu size=%llu buf=%p\n",
         (unsigned long long)offset, (unsigned long long)size, buf);
  (void)file;
  char tmp[128];
  size_t pos = 0;
  debugk("proc_uptime_read: building string, ticks=%llu\n",
         (unsigned long long)virtual_time.os_ticks);
  pos = buf_puts(tmp, sizeof(tmp), pos, "ticks: ");
  debugk("proc_uptime_read: after first buf_puts, pos=%zu\n", pos);
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.os_ticks);
  debugk("proc_uptime_read: after buf_putu64(ticks), pos=%zu\n", pos);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\ncycles: ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.system_uptime);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  debugk("proc_uptime_read: string built, len=%zu, calling copy_slice\n", pos);
  int64_t ret = copy_slice(buf, size, tmp, pos, offset);
  debugk("proc_uptime_read: copy_slice returned %lld\n", (long long)ret);
  return ret;
}

static struct file_ops_t proc_uptime_fops;

/* -------------------------------------------------------------------------
 * /proc root readdir — lists static global files.
 * ---------------------------------------------------------------------- */
static int64_t procfs_root_readdir(struct vnode_t *dir, uint32_t index,
                                   struct dentry_t **out) {
  debugk("procfs_root_readdir: index=%u\n", index);
  /* Walk the children_dentries list to find entry at position `index`. */
  uint32_t i = 0;
  list_for_each(&dir->children_dentries, pos) {
    if (i == index) {
      *out = container_of(pos, struct dentry_t, sibling_dentry);
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
  debugk("procfs_root_lookup: looking up '%s'\n", name);
  /* Search static children dentries. */
  list_for_each(&parent->children_dentries, pos) {
    struct dentry_t *d = container_of(pos, struct dentry_t, sibling_dentry);
    if (strncmp(d->name, name) == 0) {
      *out = d;
      return 0;
    }
  }
  *out = NULL;
  return -ENOENT;
}

/* -------------------------------------------------------------------------
 * Helper — create a read-only file vnode + dentry and attach it to parent.
 * ---------------------------------------------------------------------- */
static void proc_add_file(struct superblock_t *sb, struct vnode_t *parent,
                           uint32_t *id, const char *name,
                           struct file_ops_t *fops) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, (*id)++);
  vnode->permission_mode = S_IFREG | READ_EXECUTE_PERM;
  vnode->file_ops = fops;
  debugk("proc_add_file: '%s' vnode=%p file_ops=%p read_fn=%p address_space=%p\n",
         name, vnode, fops, fops ? fops->read : (void*)0, vnode->address_space);

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
  /* Initialize ops at runtime so function pointers get virtual addresses. */
  proc_uptime_fops.read = proc_uptime_read;

  struct superblock_t *sb = superblock_t_alloc();
  sb->vnode_ops.readdir = procfs_root_readdir;
  sb->vnode_ops.lookup  = procfs_root_lookup;

  /* Root vnode — a directory. */
  struct vnode_t *root = vnode_t_alloc();
  uint32_t id = 0;
  vfs_init_vnode(root, sb, id++);
  root->permission_mode = S_IFDIR | READ_EXECUTE_PERM;
  sb->root_vnode = root;

  /* Add global files. */
  proc_add_file(sb, root, &id, "uptime", &proc_uptime_fops);

  /* Root dentry. */
  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "proc", sizeof(root_dentry->name) - 1);
  root_dentry->vnode  = root;
  root_dentry->parent = NULL;
  sb->root_dentry = root_dentry;

  return sb;
}
