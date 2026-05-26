#include "kernel/filesystem/procfs/procfs.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/filesystem/mode.h"
#include "kernel/time/timer.h"
#include "kernel/memory/memory_info.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/task/task.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "errno.h"

/* -------------------------------------------------------------------------
 * /proc/kmsg — kernel log ring buffer
 * ---------------------------------------------------------------------- */
static int64_t proc_kmsg_read(struct file_t *file, uint64_t offset,
                               void *buf, uint64_t size) {
  (void)file;
  return (int64_t)klog_read((char *)buf, (size_t)size, (size_t)offset);
}

static struct file_ops_t proc_kmsg_fops;

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
/* TICKS_PER_SEC = 10 000 000 / TIMER_INTERVAL_CYCLES = 100 */
#define PROC_TICKS_PER_SEC (10000000ULL / TIMER_INTERVAL_CYCLES)

static int64_t proc_uptime_read(struct file_t *file, uint64_t offset,
                                void *buf, uint64_t size) {
  (void)file;
  uint64_t ticks = virtual_time.os_ticks;
  uint64_t secs  = ticks / PROC_TICKS_PER_SEC;
  uint64_t frac  = (ticks % PROC_TICKS_PER_SEC) * 100 / PROC_TICKS_PER_SEC;
  uint64_t idle  = virtual_time.cpu_idle;
  uint64_t isecs = idle / PROC_TICKS_PER_SEC;
  uint64_t ifrac = (idle % PROC_TICKS_PER_SEC) * 100 / PROC_TICKS_PER_SEC;

  char tmp[64];
  size_t pos = 0;
  pos = buf_putu64(tmp, sizeof(tmp), pos, secs);
  if (pos < sizeof(tmp) - 1) tmp[pos++] = '.';
  if (frac < 10 && pos < sizeof(tmp) - 1) tmp[pos++] = '0';
  pos = buf_putu64(tmp, sizeof(tmp), pos, frac);
  if (pos < sizeof(tmp) - 1) tmp[pos++] = ' ';
  pos = buf_putu64(tmp, sizeof(tmp), pos, isecs);
  if (pos < sizeof(tmp) - 1) tmp[pos++] = '.';
  if (ifrac < 10 && pos < sizeof(tmp) - 1) tmp[pos++] = '0';
  pos = buf_putu64(tmp, sizeof(tmp), pos, ifrac);
  if (pos < sizeof(tmp) - 1) tmp[pos++] = '\n';
  tmp[pos] = '\0';
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
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nMemAvailable:  ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, free_kb);
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nBuffers:       0 kB\nCached:        0 kB\nSwapTotal:     0 kB\nSwapFree:      0 kB\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_meminfo_fops;

/* -------------------------------------------------------------------------
 * /proc/version
 * ---------------------------------------------------------------------- */
static int64_t proc_version_read(struct file_t *file, uint64_t offset,
                                 void *buf, uint64_t size) {
  (void)file;
  static const char ver[] =
    "SBUnix version 1.0.0\n";
  return copy_slice(buf, size, ver, sizeof(ver) - 1, offset);
}

static struct file_ops_t proc_version_fops;

/* -------------------------------------------------------------------------
 * /proc/mounts — currently mounted filesystems
 * ---------------------------------------------------------------------- */
static int64_t proc_mounts_read(struct file_t *file, uint64_t offset,
                                void *buf, uint64_t size) {
  (void)file;
  char tmp[512];
  size_t pos = 0;
  list_for_each(&mount_list, node) {
    struct mount_t *m = container_of(node, struct mount_t, sibling_mount);
    const char *path = m->root_path[0] ? m->root_path : "/";
    const char *fstype, *device;
    const char *opts;
    if (strncmp(path, "/proc") == 0)     { fstype = "proc";   device = "proc";      opts = "ro,relatime"; }
    else if (strncmp(path, "/dev") == 0) { fstype = "devfs";  device = "devfs";     opts = "rw,relatime"; }
    else if (strncmp(path, "/mnt") == 0) { fstype = "rootfs"; device = "/dev/vda";  opts = "rw,relatime"; }
    else                                 { fstype = "tarfs";  device = "memory";    opts = "ro,relatime"; }
    /* prefer the superblock's device name if set */
    if (m->superblock->device[0])
      device = m->superblock->device;
    pos = buf_puts(tmp, sizeof(tmp), pos, device);
    pos = buf_puts(tmp, sizeof(tmp), pos, " ");
    pos = buf_puts(tmp, sizeof(tmp), pos, path);
    pos = buf_puts(tmp, sizeof(tmp), pos, " ");
    pos = buf_puts(tmp, sizeof(tmp), pos, fstype);
    pos = buf_puts(tmp, sizeof(tmp), pos, " ");
    pos = buf_puts(tmp, sizeof(tmp), pos, opts);
    pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0\n");
  }
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_mounts_fops;

/* -------------------------------------------------------------------------
 * /proc/stat — global CPU time counters
 * Format matches Linux: "cpu  user nice system idle iowait irq softirq"
 * ---------------------------------------------------------------------- */
static int64_t proc_stat_read(struct file_t *file, uint64_t offset,
                               void *buf, uint64_t size) {
  (void)file;
  char tmp[128];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, "cpu  ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.cpu_user);
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.cpu_system);
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, virtual_time.cpu_idle);
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0 0 0\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_stat_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/status
 * ---------------------------------------------------------------------- */
static const char *task_state_str(enum task_state state) {
  switch (state) {
    case TASK_RUNNING:    return "R (running)";
    case TASK_READY:      return "R (ready)";
    case TASK_BLOCKED:    return "S (sleeping)";
    case TASK_STOPPED:    return "T (stopped)";
    case TASK_ZOMBIE:     return "Z (zombie)";
    case TASK_TERMINATED: return "X (dead)";
    default:              return "? (unknown)";
  }
}

static size_t buf_puthex64(char *buf, size_t cap, size_t pos, uint64_t n) {
  static const char hex[] = "0123456789abcdef";
  char tmp[16];
  for (int i = 15; i >= 0; i--) {
    tmp[i] = hex[n & 0xf];
    n >>= 4;
  }
  for (int i = 0; i < 16 && pos < cap - 1; i++)
    buf[pos++] = tmp[i];
  buf[pos] = '\0';
  return pos;
}

static size_t buf_putoct(char *buf, size_t cap, size_t pos, uint32_t n) {
  char tmp[12];
  int i = 0;
  if (n == 0) {
    tmp[i++] = '0';
  } else {
    while (n > 0) {
      tmp[i++] = '0' + (n & 7);
      n >>= 3;
    }
  }
  while (i > 0 && pos < cap - 1)
    buf[pos++] = tmp[--i];
  buf[pos] = '\0';
  return pos;
}

/* Compute signal masks by walking the actions table. */
static void sig_masks(struct signal_state_t *ss, sigset_t *ign, sigset_t *cgt) {
  *ign = 0;
  *cgt = 0;
  for (int i = 1; i < NUM_SIGS; i++) {
    struct sigaction_t *act = ss->actions[i];
    if (act == (struct sigaction_t *)SIG_IGNORE)
      *ign |= (1ULL << (i - 1));
    else if (act != NULL && act != (struct sigaction_t *)SIG_DEFAULT_HANDLER)
      *cgt |= (1ULL << (i - 1));
  }
}

/* Virtual memory size: sum of all VMA spans in kB. */
static uint64_t vm_size_kb(struct task_t *task) {
  uint64_t bytes = 0;
  list_for_each(&task->mm_struct.vma_list, pos) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    bytes += vma->end_addr - vma->start_addr;
  }
  return bytes / 1024;
}

/* Resident set size: count valid user-space PTEs in all VMAs. */
static uint64_t vm_rss_kb(struct task_t *task) {
  uint64_t pages = 0;
  list_for_each(&task->mm_struct.vma_list, pos) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
      uint64_t pte = get_pte(task->mm_struct.root_satp, va);
      if (pte & PTE_VALID)
        pages++;
    }
  }
  return (pages * DEFAULT_PAGE_SIZE) / 1024;
}

static int64_t proc_pid_status_read(struct file_t *file, uint64_t offset,
                                    void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  sigset_t sig_ign, sig_cgt;
  sig_masks(&task->signal_state, &sig_ign, &sig_cgt);

  char tmp[1024];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, "Name:\t");
  pos = buf_puts(tmp, sizeof(tmp), pos, task->comm);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nUmask:\t");
  pos = buf_putoct(tmp, sizeof(tmp), pos, task->umask);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nState:\t");
  pos = buf_puts(tmp, sizeof(tmp), pos, task_state_str(task->state));
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nTgid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nNgid:\t0");
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nPid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nPPid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->ppid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nTracerPid:\t0");
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nUid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->uid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->uid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->uid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->uid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nGid:\t0\t0\t0\t0");
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nNStgid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nNSpid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nNSpgid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pgid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nNSsid:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->sid);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nThreads:\t1");
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nVmSize:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, vm_size_kb(task));
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nVmRSS:\t");
  pos = buf_putu64(tmp, sizeof(tmp), pos, vm_rss_kb(task));
  pos = buf_puts(tmp, sizeof(tmp), pos, " kB\nSigPnd:\t");
  pos = buf_puthex64(tmp, sizeof(tmp), pos, task->signal_state.pending);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nSigBlk:\t");
  pos = buf_puthex64(tmp, sizeof(tmp), pos, task->signal_state.blocked);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nSigIgn:\t");
  pos = buf_puthex64(tmp, sizeof(tmp), pos, sig_ign);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\nSigCgt:\t");
  pos = buf_puthex64(tmp, sizeof(tmp), pos, sig_cgt);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_pid_status_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/cmdline — NUL-separated argv
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_cmdline_read(struct file_t *file, uint64_t offset,
                                     void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;
  if (task->cmdline_len == 0)
    return 0;
  /* cmdline is NUL-separated argv; append a trailing newline for readability */
  size_t len = (size_t)task->cmdline_len;
  int64_t n = copy_slice(buf, size, task->cmdline, len, offset);
  if (n > 0 && offset + (uint64_t)n >= len) {
    /* we're at the end — append newline if there's room */
    char nl = '\n';
    int64_t r = copy_slice((char *)buf + n, size - (uint64_t)n, &nl, 1, 0);
    if (r > 0) n += r;
  }
  return n;
}

static struct file_ops_t proc_pid_cmdline_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/stat — single-line process status for ps(1)
 * Format: pid (comm) state ppid pgrp session 0 0 0 0 0 0 0 0 0 0 0 0 0 0 starttime ...
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_stat_read(struct file_t *file, uint64_t offset,
                                  void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  /* State character */
  char state_ch;
  switch (task->state) {
    case TASK_RUNNING:
    case TASK_READY:      state_ch = 'R'; break;
    case TASK_BLOCKED:    state_ch = 'S'; break;
    case TASK_STOPPED:    state_ch = 'T'; break;
    case TASK_ZOMBIE:     state_ch = 'Z'; break;
    default:              state_ch = 'X'; break;
  }

  char tmp[512];
  size_t pos = 0;
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pid);
  pos = buf_puts(tmp, sizeof(tmp), pos, " (");
  pos = buf_puts(tmp, sizeof(tmp), pos, task->comm[0] ? task->comm : "?");
  pos = buf_puts(tmp, sizeof(tmp), pos, ") ");
  if (pos < sizeof(tmp) - 1) tmp[pos++] = state_ch;
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->ppid);
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->pgid);
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->sid);
  /* tty_nr=0 tpgid=0 flags=0 minflt=0 cminflt=0 majflt=0 cmajflt=0 */
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0 0 0 0 0 0");
  /* utime stime cutime=0 cstime=0 priority=20 nice=0 num_threads=1 */
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->utime);
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, task->stime);
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0 20 0 1");
  /* itrealvalue=0 starttime=0 vsize rss */
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0 ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, vm_size_kb(task) * 1024);
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, vm_rss_kb(task) * 1024 / DEFAULT_PAGE_SIZE);
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  tmp[pos] = '\0';
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_pid_stat_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/comm — just the comm name
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_comm_read(struct file_t *file, uint64_t offset,
                                  void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;
  char tmp[32];
  size_t pos = 0;
  pos = buf_puts(tmp, sizeof(tmp), pos, task->comm[0] ? task->comm : "?");
  pos = buf_puts(tmp, sizeof(tmp), pos, "\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_pid_comm_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/statm — memory stats in pages
 * Format: size resident shared text 0 data 0
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_statm_read(struct file_t *file, uint64_t offset,
                                   void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  uint64_t vm_pages  = vm_size_kb(task) * 1024 / DEFAULT_PAGE_SIZE;
  uint64_t rss_pages = vm_rss_kb(task)  * 1024 / DEFAULT_PAGE_SIZE;

  char tmp[128];
  size_t pos = 0;
  pos = buf_putu64(tmp, sizeof(tmp), pos, vm_pages);   /* size */
  pos = buf_puts(tmp, sizeof(tmp), pos, " ");
  pos = buf_putu64(tmp, sizeof(tmp), pos, rss_pages);  /* resident */
  pos = buf_puts(tmp, sizeof(tmp), pos, " 0 0 0 0 0\n");
  return copy_slice(buf, size, tmp, pos, offset);
}

static struct file_ops_t proc_pid_statm_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/limits — rlimit table
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_limits_read(struct file_t *file, uint64_t offset,
                                    void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  static const struct { int idx; const char *name; } rows[] = {
    { 0, "Max cpu time"       },
    { 1, "Max file size"      },
    { 2, "Max data size"      },
    { 3, "Max stack size"     },
    { 4, "Max core file size" },
    { 7, "Max open files"     },
    { 9, "Max address space"  },
  };
  static const int nrows = 7;

  char tmp[1024];
  char *p = tmp;
  char *end = tmp + sizeof(tmp) - 1;

#define EMIT(s) do { const char *_s=(s); while(*_s && p<end) *p++=*_s++; } while(0)
#define EMITPAD(s, width) do { \
    const char *_s=(s); int _n=0; \
    while(*_s && p<end){*p++=*_s++;_n++;} \
    while(_n<(width) && p<end){*p++=' ';_n++;} \
  } while(0)
#define EMITU64PAD(v, width) do { \
    uint64_t _v=(v); char _b[20]; int _i=0; \
    if(_v==0){_b[_i++]='0';}else{while(_v>0){_b[_i++]='0'+(int)(_v%10);_v/=10;}} \
    int _n=0; \
    while(_i>0 && p<end){*p++=_b[--_i];_n++;} \
    while(_n<(width) && p<end){*p++=' ';_n++;} \
  } while(0)

  EMIT("Limit                     Soft Limit           Hard Limit\n");
  for (int r = 0; r < nrows; r++) {
    struct rlimit *rl = &task->rlimits.limits[rows[r].idx];
    EMITPAD(rows[r].name, 26);
    if (rl->rlim_cur == RLIM_INFINITY) { EMITPAD("unlimited", 21); }
    else { EMITU64PAD(rl->rlim_cur, 21); }
    if (rl->rlim_max == RLIM_INFINITY) { EMIT("unlimited"); }
    else { EMITU64PAD(rl->rlim_max, 0); }
    if (p < end) *p++ = '\n';
  }
#undef EMIT
#undef EMITPAD
#undef EMITU64PAD

  *p = '\0';
  return copy_slice(buf, size, tmp, (size_t)(p - tmp), offset);
}

static struct file_ops_t proc_pid_limits_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/maps — one line per VMA
 * Format: start-end perms offset 00:00 0
 * ---------------------------------------------------------------------- */
static size_t buf_puthex64_nopad(char *buf, size_t cap, size_t pos, uint64_t n) {
  static const char hex[] = "0123456789abcdef";
  char tmp[16];
  int i = 0;
  if (n == 0) {
    tmp[i++] = '0';
  } else {
    while (n > 0) { tmp[i++] = hex[n & 0xf]; n >>= 4; }
  }
  while (i > 0 && pos < cap - 1)
    buf[pos++] = tmp[--i];
  buf[pos] = '\0';
  return pos;
}

static int64_t proc_pid_maps_read(struct file_t *file, uint64_t offset,
                                  void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;

  /* Global byte position across all lines; offset/size are the window. */
  size_t global_pos = 0;
  size_t written    = 0;
  char  *dst        = (char *)buf;

  list_for_each(&task->mm_struct.vma_list, node) {
    struct vma_t *vma = container_of(node, struct vma_t, sibling_vma);

    char line[80];
    size_t pos = 0;
    pos = buf_puthex64_nopad(line, sizeof(line), pos, vma->start_addr);
    if (pos < sizeof(line) - 1) line[pos++] = '-';
    pos = buf_puthex64_nopad(line, sizeof(line), pos, vma->end_addr);
    if (pos < sizeof(line) - 1) line[pos++] = ' ';
    if (pos + 5 < sizeof(line)) {
      line[pos++] = (vma->vm_flags & VM_READ)   ? 'r' : '-';
      line[pos++] = (vma->vm_flags & VM_WRITE)  ? 'w' : '-';
      line[pos++] = (vma->vm_flags & VM_EXEC)   ? 'x' : '-';
      line[pos++] = (vma->vm_flags & VM_SHARED) ? 's' : 'p';
      line[pos++] = ' ';
    }
    pos = buf_puthex64_nopad(line, sizeof(line), pos, vma->offset);
    pos = buf_puts(line, sizeof(line), pos, " 00:00 0\n");

    /* Determine which bytes of this line fall within [offset, offset+size). */
    size_t line_end = global_pos + pos;
    if (line_end > offset && global_pos < offset + size) {
      size_t skip = (global_pos < offset) ? (size_t)(offset - global_pos) : 0;
      size_t avail = pos - skip;
      size_t take  = (avail < size - written) ? avail : size - written;
      memcpy(dst + written, line + skip, take);
      written += take;
    }
    global_pos = line_end;

    if (written >= size)
      break;
  }
  return (int64_t)written;
}

static struct file_ops_t proc_pid_maps_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/cwd — current working directory path
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_cwd_read(struct file_t *file, uint64_t offset,
                                 void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;
  char tmp[256];
  int64_t ret = vfs_dentry_get_path(task->cwd, tmp, sizeof(tmp));
  if (ret < 0)
    return ret;
  size_t len = 0;
  while (tmp[len]) len++;
  tmp[len++] = '\n';
  return copy_slice(buf, size, tmp, len, offset);
}

static struct file_ops_t proc_pid_cwd_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/exe — executable path (best-effort: argv[0] from cmdline)
 * ---------------------------------------------------------------------- */
static int64_t proc_pid_exe_read(struct file_t *file, uint64_t offset,
                                 void *buf, uint64_t size) {
  uint64_t pid = (uint64_t)file->vnode->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (task == NULL)
    return -ENOENT;
  if (task->cmdline_len == 0)
    return 0;
  /* argv[0] is the first NUL-terminated string in cmdline */
  size_t len = 0;
  while (len < (size_t)task->cmdline_len && task->cmdline[len])
    len++;
  char tmp[257];
  if (len > 256) len = 256;
  memcpy(tmp, task->cmdline, len);
  tmp[len++] = '\n';
  return copy_slice(buf, size, tmp, len, offset);
}

static struct file_ops_t proc_pid_exe_fops;

/* -------------------------------------------------------------------------
 * /proc/<pid>/fd/<n> — proxy read/write through the underlying file_t.
 * fs_private_vnode encodes (pid << 16) | fd_number.
 * ---------------------------------------------------------------------- */
static struct file_ops_t proc_fd_entry_fops;
static struct vnode_ops_t proc_fd_dir_ops;

static int64_t proc_fd_entry_read(struct file_t *file, uint64_t offset,
                                   void *buf, uint64_t size) {
  uint64_t encoded = (uint64_t)file->vnode->fs_private_vnode;
  uint64_t pid     = encoded >> 16;
  int      fd      = (int)(encoded & 0xffff);

  struct task_t *task = find_task_by_pid(pid);
  if (!task) return -ENOENT;

  struct file_t *target = find_file(&task->file_table, fd);
  if (!target || !target->file_ops || !target->file_ops->read)
    return -EBADF;

  return target->file_ops->read(target, offset, buf, size);
}

static int64_t proc_fd_entry_write(struct file_t *file, uint64_t offset,
                                    void *buf, uint64_t size) {
  uint64_t encoded = (uint64_t)file->vnode->fs_private_vnode;
  uint64_t pid     = encoded >> 16;
  int      fd      = (int)(encoded & 0xffff);

  struct task_t *task = find_task_by_pid(pid);
  if (!task) return -ENOENT;

  struct file_t *target = find_file(&task->file_table, fd);
  if (!target || !target->file_ops || !target->file_ops->write)
    return -EBADF;

  return target->file_ops->write(target, offset, buf, size);
}

/* Build a dentry for /proc/<pid>/fd/<n>. */
static struct dentry_t *proc_make_fd_dentry(struct superblock_t *sb,
                                             uint64_t pid, int fd,
                                             struct file_t *target) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, 0);

  /* Mirror the underlying file's permissions. */
  uint32_t mode = S_IFREG;
  if (target->file_ops && target->file_ops->read)  mode |= 0444;
  if (target->file_ops && target->file_ops->write) mode |= 0222;
  vnode->permission_mode  = mode;
  vnode->file_ops         = &proc_fd_entry_fops;
  vnode->fs_private_vnode = (void *)((pid << 16) | (uint64_t)fd);

  /* Name is the decimal fd number. */
  char fd_str[12];
  int i = 0, j = 0;
  char rev[12];
  int tmp = fd;
  if (tmp == 0) { rev[j++] = '0'; }
  else { while (tmp > 0) { rev[j++] = '0' + (tmp % 10); tmp /= 10; } }
  while (j > 0) fd_str[i++] = rev[--j];
  fd_str[i] = '\0';

  struct dentry_t *d = dentry_t_alloc();
  strncpy(d->name, fd_str, sizeof(d->name) - 1);
  d->vnode  = vnode;
  d->parent = NULL;
  return d;
}

static int64_t procfs_fd_readdir(struct vnode_t *dir, uint32_t index,
                                  struct dentry_t **out) {
  uint64_t pid = (uint64_t)dir->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (!task) { *out = NULL; return -ENOENT; }

  uint32_t count = 0;
  int base_fd = 0;
  list_for_each(&task->file_table.files_list, pos) {
    struct files_list_t *fl = container_of(pos, struct files_list_t, files_list);
    for (int i = 0; i < 32; i++) {
      if (fl->used_file_bitmap & (1U << i)) {
        if (count == index) {
          int fd = base_fd + i;
          *out = proc_make_fd_dentry(dir->superblock, pid, fd, fl->files[i]);
          return 0;
        }
        count++;
      }
    }
    base_fd += 32;
  }
  *out = NULL;
  return -1;
}

static int64_t procfs_fd_lookup(const char *name, struct vnode_t *parent,
                                 struct dentry_t **out) {
  uint64_t pid = (uint64_t)parent->fs_private_vnode;
  struct task_t *task = find_task_by_pid(pid);
  if (!task) { *out = NULL; return -ENOENT; }

  /* Parse name as decimal fd number. */
  int fd = 0;
  for (const char *p = name; *p; p++) {
    if (*p < '0' || *p > '9') { *out = NULL; return -ENOENT; }
    fd = fd * 10 + (*p - '0');
  }

  struct file_t *target = find_file(&task->file_table, fd);
  if (!target) { *out = NULL; return -ENOENT; }

  *out = proc_make_fd_dentry(parent->superblock, pid, fd, target);
  return 0;
}

/* Build /proc/<pid>/fd directory dentry. */
static struct dentry_t *proc_make_fd_dir_dentry(struct superblock_t *sb,
                                                  uint64_t pid) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, 0);
  vnode->permission_mode  = S_IFDIR | READ_EXECUTE_PERM;
  vnode->vnode_ops        = &proc_fd_dir_ops;
  vnode->fs_private_vnode = (void *)pid;

  struct dentry_t *d = dentry_t_alloc();
  strncpy(d->name, "fd", sizeof(d->name) - 1);
  d->vnode  = vnode;
  d->parent = NULL;
  return d;
}

/* -------------------------------------------------------------------------
 * /proc/<pid>/ directory ops
 * ---------------------------------------------------------------------- */
static struct vnode_ops_t proc_pid_dir_ops;

static struct dentry_t *proc_make_pid_file(struct superblock_t *sb,
                                            void *priv,
                                            struct file_ops_t *fops,
                                            const char *name) {
  struct vnode_t *vnode = vnode_t_alloc();
  vfs_init_vnode(vnode, sb, 0);
  vnode->permission_mode  = S_IFREG | READ_EXECUTE_PERM;
  vnode->file_ops         = fops;
  vnode->fs_private_vnode = priv;
  struct dentry_t *d = dentry_t_alloc();
  strncpy(d->name, name, sizeof(d->name) - 1);
  d->vnode = vnode; d->parent = NULL;
  return d;
}

static int64_t procfs_pid_readdir(struct vnode_t *dir, uint32_t index,
                                  struct dentry_t **out) {
  uint64_t pid = (uint64_t)dir->fs_private_vnode;
  void *priv = dir->fs_private_vnode;
  switch (index) {
    case 0:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_status_fops, "status");
      return 0;
    case 1:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_cmdline_fops, "cmdline");
      return 0;
    case 2:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_stat_fops, "stat");
      return 0;
    case 3:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_comm_fops, "comm");
      return 0;
    case 4:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_statm_fops, "statm");
      return 0;
    case 5:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_limits_fops, "limits");
      return 0;
    case 6:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_maps_fops, "maps");
      return 0;
    case 7:
      *out = proc_make_fd_dir_dentry(dir->superblock, pid);
      return 0;
    case 8:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_cwd_fops, "cwd");
      return 0;
    case 9:
      *out = proc_make_pid_file(dir->superblock, priv, &proc_pid_exe_fops, "exe");
      return 0;
    default:
      *out = NULL;
      return -1;
  }
}

static int64_t procfs_pid_lookup(const char *name, struct vnode_t *parent,
                                 struct dentry_t **out) {
  uint64_t pid = (uint64_t)parent->fs_private_vnode;
  void *priv = parent->fs_private_vnode;
  if (strncmp(name, "status") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_status_fops, "status");
    return 0;
  }
  if (strncmp(name, "cmdline") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_cmdline_fops, "cmdline");
    return 0;
  }
  if (strncmp(name, "stat") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_stat_fops, "stat");
    return 0;
  }
  if (strncmp(name, "comm") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_comm_fops, "comm");
    return 0;
  }
  if (strncmp(name, "statm") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_statm_fops, "statm");
    return 0;
  }
  if (strncmp(name, "limits") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_limits_fops, "limits");
    return 0;
  }
  if (strncmp(name, "maps") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_maps_fops, "maps");
    return 0;
  }
  if (strncmp(name, "fd") == 0) {
    *out = proc_make_fd_dir_dentry(parent->superblock, pid);
    return 0;
  }
  if (strncmp(name, "cwd") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_cwd_fops, "cwd");
    return 0;
  }
  if (strncmp(name, "exe") == 0) {
    *out = proc_make_pid_file(parent->superblock, priv, &proc_pid_exe_fops, "exe");
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

  /* "self" entry. */
  if (i == index) {
    struct dentry_t *d = proc_make_pid_dentry(dir->superblock, current_task->pid);
    strncpy(d->name, "self", sizeof(d->name) - 1);
    *out = d;
    return 0;
  }
  i++;

  /* Dynamic PID directories from the global task list — skip dead tasks. */
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->state == TASK_TERMINATED)
      continue;
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

  /* "self" resolves to the calling process's PID directory. */
  if (strncmp(name, "self") == 0) {
    struct dentry_t *d = proc_make_pid_dentry(parent->superblock, current_task->pid);
    strncpy(d->name, "self", sizeof(d->name) - 1);
    *out = d;
    return 0;
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
  proc_kmsg_fops.read         = proc_kmsg_read;
  proc_uptime_fops.read       = proc_uptime_read;
  proc_meminfo_fops.read      = proc_meminfo_read;
  proc_pid_status_fops.read   = proc_pid_status_read;
  proc_pid_cmdline_fops.read  = proc_pid_cmdline_read;
  proc_pid_stat_fops.read     = proc_pid_stat_read;
  proc_pid_comm_fops.read     = proc_pid_comm_read;
  proc_pid_statm_fops.read    = proc_pid_statm_read;
  proc_pid_limits_fops.read   = proc_pid_limits_read;
  proc_version_fops.read      = proc_version_read;
  proc_mounts_fops.read       = proc_mounts_read;
  proc_stat_fops.read         = proc_stat_read;
  proc_pid_maps_fops.read     = proc_pid_maps_read;
  proc_pid_cwd_fops.read      = proc_pid_cwd_read;
  proc_pid_exe_fops.read      = proc_pid_exe_read;
  proc_pid_dir_ops.readdir    = procfs_pid_readdir;
  proc_pid_dir_ops.lookup     = procfs_pid_lookup;
  proc_fd_entry_fops.read     = proc_fd_entry_read;
  proc_fd_entry_fops.write    = proc_fd_entry_write;
  proc_fd_dir_ops.readdir     = procfs_fd_readdir;
  proc_fd_dir_ops.lookup      = procfs_fd_lookup;

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
  proc_add_file(sb, root, &id, "kmsg",    &proc_kmsg_fops);
  proc_add_file(sb, root, &id, "mounts",  &proc_mounts_fops);
  proc_add_file(sb, root, &id, "version", &proc_version_fops);
  proc_add_file(sb, root, &id, "stat",    &proc_stat_fops);

  struct dentry_t *root_dentry = dentry_t_alloc();
  strncpy(root_dentry->name, "proc", sizeof(root_dentry->name) - 1);
  root_dentry->vnode  = root;
  root_dentry->parent = NULL;
  sb->root_dentry = root_dentry;

  return sb;
}
