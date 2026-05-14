#define DEBUG 1
#include "kernel/task/task.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "arch/riscv64/trap.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/schedule.h"
#include "kernel/signal_jump_point.h"

// Global task tracking
struct task_t *current_task = NULL;  // Currently running task
struct task_t *idle_task = NULL;     // Idle task (PID 0)
struct task_t *init_task = NULL;     // First task (PID 1)
struct list_node task_list;          // Global list of all tasks
uint64_t latest_pid = 1;             // Start at 1 since init has PID 1

void init_task_system() {
  // Initialize global task list (must be called after virtual memory is enabled)
  task_list.next = &task_list;
  task_list.prev = &task_list;
}

void set_current_task(struct task_t *task) {
  current_task = task;
  asm volatile("mv tp, %0" :: "r"(current_task));
}

void switch_to_page_table(struct task_t *task) {
  // Build satp value: mode (Sv39 = 8) in bits [63:60], PPN in bits [43:0]
  uint64_t satp = (8ULL << 60) | ((uint64_t)task->mm_struct.root_satp >> 12);

  asm volatile("sfence.vma zero, zero");

  asm volatile("csrw satp, %0" :: "r"(satp) : "memory");

  asm volatile("sfence.vma zero, zero");
}

void init_signal_state(struct task_t *task) {
  for (size_t i = 0; i < NUM_SIGS; i++) {
    task->signal_state.actions[i] = (struct sigaction_t *)SIG_DEFAULT_HANDLER;
  }
  task->signal_state.pending = 0;
  task->signal_state.blocked = 0;
  task->signal_handler_depth = 0;
}

void init_files(struct files_table_t *files_table) {
  files_table->files_list.next = &files_table->files_list;
  files_table->files_list.prev = &files_table->files_list;

  struct files_list_t *files_list = files_list_t_alloc();
  files_list->used_file_bitmap = 1 | 1 << 1 | 1 << 2;  // Mark FDs 0, 1, 2 as used

  struct file_t *stdin, *stdout, *stderr;
  vfs_open("/dev/tty", O_RDONLY, &stdin);
  vfs_open("/dev/tty", O_WRONLY, &stdout);
  vfs_open("/dev/tty", O_WRONLY, &stderr);

  files_list->files[0] = stdin;
  files_list->files[1] = stdout;
  files_list->files[2] = stderr;

  list_append(&files_table->files_list, &files_list->files_list);
}

void allocate_kernel_stack(struct task_t *task) {
  // All processes have their kernel stack at the same VA, but different physical pages
  void *phys_page1 = get_page(true);
  void *phys_page2 = get_page(true);
  void *phys_page3 = get_page(true);
  void *phys_page4 = get_page(true);

  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE,
           (uint64_t)phys_page1, PTE_VALID | PTE_R | PTE_W);
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE + 4096,
           (uint64_t)phys_page2, PTE_VALID | PTE_R | PTE_W);
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE + 8192,
           (uint64_t)phys_page3, PTE_VALID | PTE_R | PTE_W);
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE + 12288,
           (uint64_t)phys_page4, PTE_VALID | PTE_R | PTE_W);

  task->kernel_context.stack_start = KERNEL_STACK_VIRTUAL_BASE;
  task->kernel_context.sp = KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE;

  // Map signal jump point page (shared across all processes)
  void *jump_point_page = get_signal_jump_point_page();
  if (jump_point_page) {
    debugk("task: mapping signal_jump_point page for PID %llu: vaddr=%llx -> paddr=%p (flags: U|R|X)\n",
           task->pid, SIGNAL_JUMP_POINT_ADDR, jump_point_page);
    map_page(task->mm_struct.root_satp, SIGNAL_JUMP_POINT_ADDR,
             (uint64_t)jump_point_page, PTE_VALID | PTE_U | PTE_R | PTE_X);
  } else {
    debugk("task: WARNING - signal_jump_point page is NULL for PID %llu!\n", task->pid);
  }
}

struct task_t *task_init() {
  struct task_t *task = task_t_alloc();
  task->pid = 1;
  task->ppid = 0;  // Init's parent is kernel (PID 0 / idle)
  task->pgid = 1;  // Init is its own process group leader
  task->uid = 0;
  task->state = TASK_READY;

  // Initialize page table with kernel mappings copied from root
  task->mm_struct.root_satp = init_new_page_table();

  // Initialize VMA list (empty circular list)
  task->mm_struct.vma_list.next = &task->mm_struct.vma_list;
  task->mm_struct.vma_list.prev = &task->mm_struct.vma_list;

  init_files(&(task->file_table));

  // Allocate kernel stack
  allocate_kernel_stack(task);

  // Set return address for when this task is first scheduled
  // switch_to() will restore this ra and ret to it
  // This makes new tasks jump to fresh_task_jump() on first schedule
  task->kernel_context.ra = (uint64_t)fresh_task_jump;

  task->exit_status = 0;
  task->wait_reason = WAIT_NONE;
  task->wait_pid = 0;
  task->runtime = 0;
  task->max_runtime = MAX_RUNTIME;

  // Initialize signal state
  init_signal_state(task);

  return task;
}

// Idle loop - runs when no other tasks are ready
void idle_loop(void) {
  extern void enable_interrupts(void);

  asm volatile("csrw sscratch, zero");

  enable_interrupts();

  while (1) {
    asm volatile("wfi");
  }
}

void create_idle_task(void) {
  idle_task = task_t_alloc();
  idle_task->pid = 0;
  idle_task->ppid = 0; 
  idle_task->pgid = 0;
  idle_task->uid = 0;
  idle_task->state = TASK_RUNNING;

  idle_task->mm_struct.root_satp = init_new_page_table();

  idle_task->mm_struct.vma_list.next = &idle_task->mm_struct.vma_list;
  idle_task->mm_struct.vma_list.prev = &idle_task->mm_struct.vma_list;

  allocate_kernel_stack(idle_task);

  idle_task->kernel_context.ra = (uint64_t)idle_loop;

  idle_task->cwd = base_mount->superblock->root_dentry;

  idle_task->exit_status = 0;
  idle_task->wait_reason = WAIT_NONE;
  idle_task->wait_pid = 0;
  idle_task->runtime = 0;
  idle_task->max_runtime = 0;

  // Initialize signal state
  init_signal_state(idle_task);

  idle_task->scheduler_list.next = &idle_task->scheduler_list;
  idle_task->scheduler_list.prev = &idle_task->scheduler_list;
  idle_task->wait_list.next = &idle_task->wait_list;
  idle_task->wait_list.prev = &idle_task->wait_list;
}

void create_init_process() {
  init_task_system();
  init_task = task_init();
  load_elf(init_task , "/bin/init");
  list_append(&task_list, &init_task->task_list);
  init_task->cwd = base_mount->superblock->root_dentry;
  set_current_task(init_task);
  init_task->state = TASK_RUNNING;
}

void start_init_process();

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

  size_t vaddr_aligned = vaddr & ~(DEFAULT_PAGE_SIZE - 1);
  size_t offset_aligned = offset & ~(DEFAULT_PAGE_SIZE - 1);
  size_t offset_in_page = vaddr - vaddr_aligned;
  size_t total_size = offset_in_page + size;
  size_t num_pages = (total_size + DEFAULT_PAGE_SIZE - 1) / DEFAULT_PAGE_SIZE;
  size_t vaddr_end = vaddr_aligned + (num_pages * DEFAULT_PAGE_SIZE);
  for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    struct vma_t *existing = find_vma(mm_struct, va);
    if (existing != NULL) {
      return -1;
    }
  }

  struct vma_t *new_vma = vma_t_alloc();
  new_vma->start_addr = vaddr_aligned;
  new_vma->end_addr = vaddr_end;
  new_vma->backing_file = vnode;
  new_vma->offset = offset_aligned;
  new_vma->vm_flags = vm_flags;

  vnode->refcount++;

  list_append(&mm_struct->vma_list, &new_vma->sibling_vma);

  // Eagerly load the pages into the page table from the file
  if (eager) {
    size_t file_offset = offset_aligned;
    for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
      void *phys_page = vfs_get_page(vnode, file_offset, VFS_PAGE_REF);

      // Convert VM flags to PTE flags
      // Note: map_page() sets PTE_VALID and PTE_A automatically
      uint64_t pte_flags = PTE_U;
      if (vm_flags & VM_READ)  pte_flags |= PTE_R;
      if (vm_flags & VM_WRITE) pte_flags |= PTE_W;
      if (vm_flags & VM_EXEC)  pte_flags |= PTE_X;

      map_page(mm_struct->root_satp, va, (uint64_t)phys_page, pte_flags);
      file_offset += DEFAULT_PAGE_SIZE;
    }
  }

  return 0;
}

int64_t anon_memory_map(struct mm_struct_t *mm_struct, size_t vaddr,
                                size_t size, uint64_t vm_flags, bool eager) {

  if (mm_struct == NULL || size == 0) {
    panic("anon_memory_map: invalid parameters\n");
  }

  size_t vaddr_aligned = vaddr & ~(DEFAULT_PAGE_SIZE - 1);
  size_t num_pages = (size + DEFAULT_PAGE_SIZE - 1) / DEFAULT_PAGE_SIZE;
  size_t vaddr_end = vaddr_aligned + (num_pages * DEFAULT_PAGE_SIZE);

  for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
    struct vma_t *existing = find_vma(mm_struct, va);
    if (existing != NULL) {
      return -1;
    }
  }

  struct vma_t *new_vma = vma_t_alloc();
  new_vma->start_addr = vaddr_aligned;
  new_vma->end_addr = vaddr_end;
  new_vma->backing_file = NULL;
  new_vma->offset = 0;
  new_vma->vm_flags = vm_flags;

  list_append(&mm_struct->vma_list, &new_vma->sibling_vma);

  // Eagerly allocate and map anonymous pages
  if (eager) {
    for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
      void *phys_page = get_zero_page(false);
      if (phys_page == NULL) {
        panic("anon_memory_map: failed to allocate zero page\n");
      }

      // Convert VM flags to PTE flags
      // Note: map_page() sets PTE_VALID and PTE_A automatically
      uint64_t pte_flags = PTE_U;
      if (vm_flags & VM_READ)  pte_flags |= PTE_R;
      if (vm_flags & VM_WRITE) pte_flags |= PTE_W;
      if (vm_flags & VM_EXEC)  pte_flags |= PTE_X;

      map_page(mm_struct->root_satp, va, (uint64_t)phys_page, pte_flags);
    }
  }

  return 0;
}

struct vma_t *copy_vma(struct vma_t *vma, struct task_t *old_task, struct task_t *new_task) {
  struct vma_t *new_vma = vma_t_alloc();
  new_vma->start_addr = vma->start_addr;
  new_vma->end_addr = vma->end_addr;
  new_vma->vm_flags = vma->vm_flags;
  new_vma->backing_file = vma->backing_file;
  new_vma->offset = vma->offset;

  if (vma->backing_file != NULL) {
    // File-backed VMA: share pages and increment refcounts
    vma->backing_file->refcount++;
    vfs_address_space_inc_ref(vma->start_addr, vma->end_addr, vma->offset, vma->backing_file->address_space);

    // Map the same physical pages (share them)
    for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
      uint64_t old_pte = get_pte(old_task->mm_struct.root_satp, va);

      if (!(old_pte & PTE_VALID)) {
        continue;
      }

      void *old_phys_page = (void *)PTE_DECODE(old_pte);
      uint64_t pte_flags = old_pte & (PTE_R | PTE_W | PTE_X | PTE_U);
      map_page(new_task->mm_struct.root_satp, va, (uint64_t)old_phys_page, pte_flags);
    }
  } else {
    // Anonymous VMA: copy pages
    for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
      uint64_t old_pte = get_pte(old_task->mm_struct.root_satp, va);

      if (!(old_pte & PTE_VALID)) {
        continue;
      }

      void *new_phys_page = get_page(false);
      if (!new_phys_page) {
        panic("copy_vma: failed to allocate page");
      }

      void *old_phys_page = (void *)PTE_DECODE(old_pte);

      void *old_virt = PHYS_TO_VIRT(old_phys_page);
      void *new_virt = PHYS_TO_VIRT(new_phys_page);
      memcpy(new_virt, old_virt, DEFAULT_PAGE_SIZE);

      uint64_t pte_flags = old_pte & (PTE_R | PTE_W | PTE_X | PTE_U);
      map_page(new_task->mm_struct.root_satp, va, (uint64_t)new_phys_page, pte_flags);
    }
  }

  return new_vma;
}

void copy_mm(struct task_t *old_task, struct task_t *new_task) {
  // the root_satp in the mm_struct should be setup before this
  list_for_each(&old_task->mm_struct.vma_list, pos) {
    struct vma_t *old_vma = container_of(pos, struct vma_t, sibling_vma);
    struct vma_t *new_vma = copy_vma(old_vma, old_task, new_task);
    list_append(&new_task->mm_struct.vma_list, &new_vma->sibling_vma);
  }
  new_task->mm_struct.entry_addr = old_task->mm_struct.entry_addr;
}

void copy_file_table(struct files_table_t *old_table, struct files_table_t *new_table) {
  new_table->files_list.next = &new_table->files_list;
  new_table->files_list.prev = &new_table->files_list;

  list_for_each(&old_table->files_list, pos) {
    struct files_list_t *old_files_list = container_of(pos, struct files_list_t, files_list);

    struct files_list_t *new_files_list = files_list_t_alloc();

    new_files_list->used_file_bitmap = old_files_list->used_file_bitmap;

    for (int i = 0; i < 32; i++) {
      new_files_list->files[i] = old_files_list->files[i];

      if (old_files_list->files[i] != NULL) {
        old_files_list->files[i]->refcount++;
      }
    }

    list_append(&new_table->files_list, &new_files_list->files_list);
  }
}

// Find task by PID
struct task_t *find_task_by_pid(uint64_t pid) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);
    if (task->pid == pid) {
      return task;
    }
  }
  return NULL;
}

// Check if task has alive children (not zombies)
bool has_alive_children(struct task_t *parent, int64_t specific_pid) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);

    if (task->ppid == parent->pid && task->state != TASK_ZOMBIE) {
      if (specific_pid == -1 || task->pid == (uint64_t)specific_pid) {
        return true;
      }
    }
  }
  return false;
}

// Find zombie child matching criteria
struct task_t *find_zombie_child(struct task_t *parent, int64_t specific_pid) {
  list_for_each(&task_list, pos) {
    struct task_t *task = container_of(pos, struct task_t, task_list);

    if (task->ppid == parent->pid && task->state == TASK_ZOMBIE) {
      if (specific_pid == -1 || task->pid == (uint64_t)specific_pid) {
        return task;
      }
    }
  }
  return NULL;
}

// Reap zombie task - free all resources
void reap_zombie(struct task_t *zombie) {
  uint64_t zombie_pid = zombie->pid;  // Save PID before freeing
  debugk("reap_zombie: STARTING for PID %llu\n", zombie_pid);

  // Sanity check: make sure we're not trying to reap the current task
  if (zombie == current_task) {
    panic("reap_zombie: Attempted to reap current_task!");
  }

  // Remove from global task list
  debugk("reap_zombie: Removing from task list\n");
  list_remove(&zombie->task_list);

  // Free kernel stack pages (4 pages at KERNEL_STACK_VIRTUAL_BASE)
  // Interrupt protection is handled inside free_page()
  debugk("reap_zombie: Freeing kernel stack pages\n");
  // for (uint64_t va = KERNEL_STACK_VIRTUAL_BASE; va < KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE; va += DEFAULT_PAGE_SIZE) {
  //   uint64_t pte = get_pte(zombie->mm_struct.root_satp, va);
  //   if (pte & PTE_VALID) {
  //     void *phys_page = (void *)PTE_DECODE(pte);
  //     debugk("  Freeing kernel stack page at va=%llx, phys=%p\n", va, phys_page);
  //     free_page(phys_page);
  //     debugk("  Unmapping kernel stack page at va=%llx\n", va);
  //     unmap_page(zombie->mm_struct.root_satp, va);
  //   }
  // }
  debugk("reap_zombie: Done freeing kernel stack pages\n");

  // Free the root page table itself
  debugk("reap_zombie: Freeing root page table phys=%p\n", zombie->mm_struct.root_satp);
  //free_page(zombie->mm_struct.root_satp);
  debugk("reap_zombie: Done freeing root page table\n");

  // Free the task structure
  debugk("reap_zombie: Freeing task structure\n");
  //task_t_free(zombie);
  debugk("reap_zombie: COMPLETED for PID %llu\n", zombie_pid);  // Use saved PID
}

void task_cleanup(int exit_status) {
  debugk("task_cleanup: PID %llu terminating with status %d\n", current_task->pid, exit_status);

  current_task->exit_status = exit_status;
  debugk("task_cleanup: closing all files for PID %llu\n", current_task->pid);
  close_all_files(current_task);
  debugk("task_cleanup: clearing VMAs for PID %llu\n", current_task->pid);
  // clear_vmas(current_task);
  debugk("task_cleanup: marking PID %llu as ZOMBIE\n", current_task->pid);
  current_task->state = TASK_ZOMBIE;

  debugk("task_cleanup: looking for parent (PPID %llu)\n", current_task->ppid);
  struct task_t *parent = find_task_by_pid(current_task->ppid);
  if (parent) {
    debugk("task_cleanup: found parent PID %llu (state=%d, wait_reason=%d, wait_pid=%lld)\n",
           parent->pid, parent->state, parent->wait_reason, parent->wait_pid);
    if (parent->state == TASK_BLOCKED && parent->wait_reason == WAIT_CHILD) {
      if (parent->wait_pid == -1 || parent->wait_pid == (int64_t)current_task->pid) {
        debugk("task_cleanup: waking parent PID %llu\n", parent->pid);
        unblock_task(parent);
        debugk("task_cleanup: parent PID %llu woken\n", parent->pid);
      } else {
        debugk("task_cleanup: parent waiting for different PID (%lld), not waking\n", parent->wait_pid);
      }
    } else {
      debugk("task_cleanup: parent not waiting (state=%d, wait_reason=%d)\n", parent->state, parent->wait_reason);
    }
  } else {
    debugk("task_cleanup: parent PID %llu not found\n", current_task->ppid);
  }
  debugk("task_cleanup: done for PID %llu\n", current_task->pid);
}

void fork_sig_copy(struct signal_state_t *signal_state) {
  struct sigaction_t *old_sigaction, *new_sigaction;
  for (size_t i = 0; i < NUM_SIGS; i++) {
    old_sigaction = current_task->signal_state.actions[i];
    if (old_sigaction == (struct sigaction_t *)SIG_DEFAULT_HANDLER ||
        old_sigaction == (struct sigaction_t *)SIG_IGNORE) {
      signal_state->actions[i] = old_sigaction;
    } else if (old_sigaction != NULL) {
      new_sigaction = sigaction_t_alloc();
      memcpy(new_sigaction, old_sigaction, sizeof(struct sigaction_t));
      signal_state->actions[i] = new_sigaction;
    } else {
      signal_state->actions[i] = NULL;
    }
  }
  signal_state->pending = 0;
  signal_state->blocked = current_task->signal_state.blocked;
}

uint64_t fork_off() {
  // Should create a complete copy of the address space of the current task
  // For now we will manually copy over everything on this call  TODO: add copy on write
  struct task_t *new_task = task_t_alloc();
  new_task->ppid = current_task->pid;
  new_task->pid = ++latest_pid;
  new_task->pgid = current_task->pgid;  // Inherit process group from parent
  new_task->uid = current_task->uid;
  new_task->cwd = current_task->cwd;

  new_task->mm_struct.root_satp = init_new_page_table();
  new_task->mm_struct.vma_list.next = &new_task->mm_struct.vma_list;
  new_task->mm_struct.vma_list.prev = &new_task->mm_struct.vma_list;

  fork_sig_copy(&new_task->signal_state);

  allocate_kernel_stack(new_task);

  new_task->kernel_context.ra = (uint64_t)fresh_task_jump;
  for (int i = 0; i < 12; i++) {
    new_task->kernel_context.s[i] = 0;
  }

  copy_file_table(&current_task->file_table, &new_task->file_table);
  copy_mm(current_task, new_task);

  memcpy(&new_task->tf, &current_task->tf, sizeof(struct trap_frame));
  current_task->tf.a0 = new_task->pid;
  new_task->tf.a0 = 0;

  new_task->state = TASK_READY;

  // Initialize wait/exit fields
  new_task->exit_status = 0;
  new_task->wait_reason = WAIT_NONE;
  new_task->wait_pid = 0;
  new_task->runtime = 0;
  new_task->max_runtime = MAX_RUNTIME;
  new_task->signal_handler_depth = 0;

  list_append(&task_list, &new_task->task_list);
  list_append(scheduler.active_list, &new_task->scheduler_list);

  return new_task->pid;
}

struct file_t *find_file(struct files_table_t *file_table, int fd) {
  struct file_t *file = NULL;
  list_for_each(&file_table->files_list, pos) {
    struct files_list_t *files_list = container_of(pos, struct files_list_t, files_list);

    // Check if this fd is marked as used in the bitmap
    if (files_list->used_file_bitmap & (1 << fd)) {
      file = files_list->files[fd];
      break;
    }
  }
  return file;
}

int alloc_fd(struct files_table_t *file_table, struct file_t *file) {
  struct files_list_t *files_list = NULL;
  int fd = -1;

  list_for_each(&file_table->files_list, pos) {
    files_list = container_of(pos, struct files_list_t, files_list);

    for (int i = 0; i < 32; i++) {
      if (!(files_list->used_file_bitmap & (1 << i))) {
        fd = i;
        break;
      }
    }
    if (fd != -1) break;
  }

  if (fd == -1) {
    files_list = files_list_t_alloc();
    if (!files_list) {
      return -1;
    }
    files_list->used_file_bitmap = 0;
    list_append(&file_table->files_list, &files_list->files_list);
    fd = 0;
  }

  files_list->used_file_bitmap |= (1 << fd);
  files_list->files[fd] = file;

  return fd;
}

void clear_vmas(struct task_t *task) {
  debugk("clear_vmas: task=%p\n", task);
  if (!task || !task->mm_struct.root_satp) {
    debugk("clear_vmas: task or root_satp is NULL, returning\n");
    return;
  }

  debugk("clear_vmas: task->mm_struct.root_satp=%p\n", task->mm_struct.root_satp);
  debugk("clear_vmas: vma_list sentinel=%p\n", &task->mm_struct.vma_list);

  struct list_node *pos = task->mm_struct.vma_list.next;
  while (pos != &task->mm_struct.vma_list) {
    struct vma_t *vma = container_of(pos, struct vma_t, sibling_vma);
    struct list_node *next = pos->next;

    if (vma->backing_file == NULL) {
      debugk("clear_vmas: anonymous VMA [%llx-%llx]\n", vma->start_addr, vma->end_addr);
      for (uint64_t va = vma->start_addr; va < vma->end_addr; va += DEFAULT_PAGE_SIZE) {
        uint64_t pte = get_pte(task->mm_struct.root_satp, va);
        if (pte & PTE_VALID) {
          void *phys_page = (void *)PTE_DECODE(pte);
          debugk("  Freeing anonymous page at va=%llx, phys=%p\n", va, phys_page);
          free_page(phys_page);
        }
        unmap_page(task->mm_struct.root_satp, va);
      }
    } else {
      debugk("clear_vmas: file-backed VMA [%llx-%llx], vnode refcount before=%llu\n",
             vma->start_addr, vma->end_addr, vma->backing_file->refcount);
      vfs_address_space_drop_ref(vma->start_addr, vma->end_addr, vma->offset, vma->backing_file->address_space);
      unmap_pages(task->mm_struct.root_satp, vma->start_addr, vma->end_addr);
      vma->backing_file->refcount--;
      debugk("  vnode refcount after=%llu\n", vma->backing_file->refcount);
    }

    list_remove(&vma->sibling_vma);
    vma_t_free(vma);

    pos = next;
  }

  asm volatile("sfence.vma zero, zero");
}

void close_all_files(struct task_t *task) {
  debugk("task pointer: %p\n", task);
  debugk("current_task pointer: %p\n", current_task);
  debugk("file_table offset in task_t: %d\n", (int)((char*)&task->file_table - (char*)task));
  debugk("file_table sentinel address: %p\n", &task->file_table.files_list);
  struct list_node *pos = task->file_table.files_list.next;
  debugk("Initial pos=%p, pos->next=%p\n", pos, pos->next);
  while (pos != &task->file_table.files_list) {
    debugk("Loop: pos=%p, pos->next=%p, sentinel=%p\n", pos, pos->next, &task->file_table.files_list);
    struct files_list_t *files_list = container_of(pos, struct files_list_t, files_list);
    debugk("files_list=%p\n", files_list);
    struct list_node *next = pos->next;

    for (int i = 0; i < 32; i++) {
      if (files_list->files[i] != NULL) {
        files_list->files[i]->refcount--;
        if (files_list->files[i]->refcount == 0) {
          file_t_free(files_list->files[i]);
        }
        files_list->files[i] = NULL;
      }
    }

    list_remove(&files_list->files_list);
    files_list_t_free(files_list);

    pos = next;
  }
}
