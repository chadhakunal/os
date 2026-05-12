#include "kernel/tests/vfs_test.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "lib/string.h"
#include "lib/list.h"
#include "lib/printk/printk.h"
#include "errno.h"

/* --------------------------------------------------------------------------
 * Minimal test framework
 * ----------------------------------------------------------------------- */

static int vfs_test_pass_count = 0;
static int vfs_test_fail_count = 0;

#define CHECK(cond, name) do { \
  if (cond) { \
    printk("  [PASS] %s\n", name); \
    vfs_test_pass_count++; \
  } else { \
    printk("  [FAIL] %s\n", name); \
    vfs_test_fail_count++; \
  } \
} while (0)

/* Resolve an absolute path; returns the vnode (through mounted_vnode if set),
 * or NULL on failure. */
static struct vnode_t *resolve_vnode(const char *path) {
  struct dentry_t *d;
  if (vfs_resolve_path(path, &d) < 0 || d == NULL || d->vnode == NULL)
    return NULL;
  return d->vnode->mounted_vnode ? d->vnode->mounted_vnode : d->vnode;
}

/* --------------------------------------------------------------------------
 * Helpers to wire a new dentry into the VFS cache after create/mkdir
 * ----------------------------------------------------------------------- */
static void attach_dentry(struct vnode_t *parent_vnode,
                           struct dentry_t *parent_dentry,
                           struct dentry_t *child) {
  child->parent = parent_dentry;
  list_append(&parent_vnode->children_dentries, &child->sibling_dentry);
}

/* --------------------------------------------------------------------------
 * Individual tests
 * ----------------------------------------------------------------------- */

static void test_lookup_existing(void) {
  printk("\n-- Lookup existing file --\n");
  struct dentry_t *d;
  int ret = vfs_resolve_path("/mnt/hello.txt", &d);
  CHECK(ret == 0 && d != NULL && d->vnode != NULL, "lookup /mnt/hello.txt");
}

static void test_read_existing(void) {
  printk("\n-- Read existing file --\n");
  struct file_t *file;
  int ret = vfs_open("/mnt/hello.txt", O_RDONLY, &file);
  CHECK(ret == 0 && file != NULL, "open /mnt/hello.txt");
  if (ret != 0 || file == NULL) return;

  char buf[64];
  memset(buf, 0, sizeof(buf));
  int64_t n = vfs_read(file, 0, buf, sizeof(buf) - 1);
  CHECK(n > 0, "read returns > 0 bytes");
  CHECK(strncmp(buf, "Hello from sbfs!\n") == 0,
        "content matches 'Hello from sbfs!'");
}

static void test_mkdir(struct vnode_t *mnt_vnode, struct dentry_t *mnt_dentry,
                       struct dentry_t **out_dir_dentry) {
  printk("\n-- mkdir --\n");
  /* Clean up from any previous run that left this directory on disk. */
  vfs_rmdir("vfstest_dir", mnt_vnode);

  struct dentry_t *d;
  int ret = vfs_mkdir("vfstest_dir", mnt_vnode, &d);
  CHECK(ret == 0 && d != NULL, "mkdir vfstest_dir");
  if (ret == 0 && d != NULL)
    attach_dentry(mnt_vnode, mnt_dentry, d);
  *out_dir_dentry = (ret == 0) ? d : NULL;
}

static void test_lookup_after_mkdir(void) {
  printk("\n-- Lookup after mkdir --\n");
  struct vnode_t *v = resolve_vnode("/mnt/vfstest_dir");
  CHECK(v != NULL && IS_DIR(v->permission_mode), "lookup /mnt/vfstest_dir");
}

static void test_create_and_write(struct vnode_t *dir_vnode,
                                   struct dentry_t *dir_dentry,
                                   struct dentry_t **out_file_dentry) {
  printk("\n-- Create and write file --\n");
  struct dentry_t *d;
  int ret = vfs_create("test.txt", dir_vnode, &d);
  CHECK(ret == 0 && d != NULL, "create test.txt");
  if (ret != 0 || d == NULL) { *out_file_dentry = NULL; return; }
  attach_dentry(dir_vnode, dir_dentry, d);
  *out_file_dentry = d;

  const char *content = "Hello VFS test!";
  int32_t written = vfs_vnode_write(d->vnode, content, 15, 0);
  CHECK(written == 15, "write 15 bytes to test.txt");
}

static void test_read_back(struct vnode_t *file_vnode) {
  printk("\n-- Read back written file --\n");
  if (file_vnode == NULL) {
    CHECK(0, "read back (skipped: no vnode)");
    return;
  }
  char buf[32];
  memset(buf, 0, sizeof(buf));
  int32_t n = vfs_vnode_read(file_vnode, buf, 15, 0);
  CHECK(n == 15, "read returns 15 bytes");
  CHECK(strncmp(buf, "Hello VFS test!") == 0, "read content matches");
}

static void test_readdir(struct vnode_t *dir_vnode) {
  printk("\n-- readdir --\n");
  struct dentry_t *entry;
  int ret = vfs_readdir(dir_vnode, 0, &entry);
  CHECK(ret == 0 && entry != NULL, "readdir index 0 returns entry");
  if (ret == 0 && entry != NULL)
    CHECK(strncmp(entry->name, "test.txt") == 0, "readdir[0].name == test.txt");

  /* index 1 should be out of range */
  struct dentry_t *entry2;
  ret = vfs_readdir(dir_vnode, 1, &entry2);
  CHECK(ret != 0, "readdir index 1 returns error (only 1 entry)");
}

static void test_lookup_created_file(void) {
  printk("\n-- Lookup created file by path --\n");
  struct vnode_t *v = resolve_vnode("/mnt/vfstest_dir/test.txt");
  CHECK(v != NULL && IS_REG(v->permission_mode),
        "lookup /mnt/vfstest_dir/test.txt");
}

static void test_unlink(struct vnode_t *dir_vnode) {
  printk("\n-- unlink --\n");
  int ret = vfs_unlink("test.txt", dir_vnode);
  CHECK(ret == 0, "unlink test.txt returns 0");
}

static void test_rmdir_empty(struct vnode_t *mnt_vnode) {
  printk("\n-- rmdir empty dir --\n");
  int ret = vfs_rmdir("vfstest_dir", mnt_vnode);
  CHECK(ret == 0, "rmdir vfstest_dir (empty) returns 0");
}

static void test_rmdir_nonempty(struct vnode_t *mnt_vnode,
                                 struct dentry_t *mnt_dentry) {
  printk("\n-- rmdir non-empty dir --\n");
  struct dentry_t *d;
  int ret = vfs_mkdir("vfstest_nonempty", mnt_vnode, &d);
  if (ret != 0 || d == NULL) { CHECK(0, "setup: mkdir vfstest_nonempty"); return; }
  attach_dentry(mnt_vnode, mnt_dentry, d);

  struct dentry_t *f;
  vfs_create("file.txt", d->vnode, &f);
  if (f) attach_dentry(d->vnode, d, f);

  ret = vfs_rmdir("vfstest_nonempty", mnt_vnode);
  CHECK(ret == -ENOTEMPTY, "rmdir non-empty returns -ENOTEMPTY");

  /* cleanup */
  vfs_unlink("file.txt", d->vnode);
  vfs_rmdir("vfstest_nonempty", mnt_vnode);
}

static void test_create_duplicate(struct vnode_t *mnt_vnode) {
  printk("\n-- Create duplicate file --\n");
  struct dentry_t *d1, *d2;
  vfs_create("dup.txt", mnt_vnode, &d1);
  int ret = vfs_create("dup.txt", mnt_vnode, &d2);
  CHECK(ret == -EEXIST, "second create returns -EEXIST");
  if (d1) vfs_unlink("dup.txt", mnt_vnode);
}

static void test_unlink_directory(struct vnode_t *mnt_vnode,
                                   struct dentry_t *mnt_dentry) {
  printk("\n-- Unlink on directory returns -EISDIR --\n");
  struct dentry_t *d;
  int ret = vfs_mkdir("vfstest_isdir", mnt_vnode, &d);
  if (ret != 0 || d == NULL) { CHECK(0, "setup: mkdir vfstest_isdir"); return; }
  attach_dentry(mnt_vnode, mnt_dentry, d);

  ret = vfs_unlink("vfstest_isdir", mnt_vnode);
  CHECK(ret == -EISDIR, "unlink on dir returns -EISDIR");

  vfs_rmdir("vfstest_isdir", mnt_vnode);
}

/* --------------------------------------------------------------------------
 * Entry point
 * ----------------------------------------------------------------------- */

void vfs_test_run(void) {
  vfs_test_pass_count = 0;
  vfs_test_fail_count = 0;

  printk("\n========== VFS TESTS ==========\n");

  /* Resolve /mnt mount point once; all sbfs tests hang off it. */
  struct dentry_t *mnt_dentry;
  if (vfs_resolve_path("/mnt", &mnt_dentry) < 0 || mnt_dentry == NULL) {
    printk("[VFS TEST] ERROR: could not resolve /mnt — aborting tests\n");
    return;
  }
  struct vnode_t *mnt_vnode = mnt_dentry->vnode->mounted_vnode
                               ? mnt_dentry->vnode->mounted_vnode
                               : mnt_dentry->vnode;

  test_lookup_existing();
  test_read_existing();

  struct dentry_t *dir_dentry = NULL;
  test_mkdir(mnt_vnode, mnt_dentry, &dir_dentry);
  test_lookup_after_mkdir();

  struct dentry_t *file_dentry = NULL;
  if (dir_dentry != NULL) {
    test_create_and_write(dir_dentry->vnode, dir_dentry, &file_dentry);
    test_read_back(file_dentry ? file_dentry->vnode : NULL);
    test_readdir(dir_dentry->vnode);
    test_lookup_created_file();
    test_unlink(dir_dentry->vnode);
    test_rmdir_empty(mnt_vnode);
  }

  test_rmdir_nonempty(mnt_vnode, mnt_dentry);
  test_create_duplicate(mnt_vnode);
  test_unlink_directory(mnt_vnode, mnt_dentry);

  printk("\n========== RESULTS: %d passed, %d failed ==========\n\n",
         vfs_test_pass_count, vfs_test_fail_count);
}
