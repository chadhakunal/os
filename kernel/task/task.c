#include "kernel/task/task.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/memory/page_tables.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/schedule.h"

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

/* Set the current task and update tp register */
void set_current_task(struct task_t *task) {
  current_task = task;
  // Update tp register to point to current_task
  // This allows trap_vector to access current_task->tf directly
  asm volatile("mv tp, %0" :: "r"(current_task));
}

/* Switch to a task's page table */
void switch_to_page_table(struct task_t *task) {
  // Build satp value: mode (Sv39 = 8) in bits [63:60], PPN in bits [43:0]
  uint64_t satp = (8ULL << 60) | ((uint64_t)task->mm_struct.root_satp >> 12);

  // Flush TLB before switching
  asm volatile("sfence.vma zero, zero");

  // Switch page table
  asm volatile("csrw satp, %0" :: "r"(satp) : "memory");

  // Flush TLB after switching
  asm volatile("sfence.vma zero, zero");
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

  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE,
           (uint64_t)phys_page1, PTE_VALID | PTE_R | PTE_W);
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE + 4096,
           (uint64_t)phys_page2, PTE_VALID | PTE_R | PTE_W);

  task->kernel_context.stack_start = KERNEL_STACK_VIRTUAL_BASE;
  task->kernel_context.sp = KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE;
}

struct task_t *task_init() {
  struct task_t *task = task_t_alloc();
  task->pid = 1;
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

  return task;
}

// Idle loop - runs when no other tasks are ready
void idle_loop(void) {
  while (1) {
    // Wait for interrupt - saves power and yields CPU
    asm volatile("wfi");
  }
}

// Create idle task (PID 0) - runs when nothing else can run
void create_idle_task(void) {
  idle_task = task_t_alloc();
  idle_task->pid = 0;
  idle_task->uid = 0;
  idle_task->state = TASK_RUNNING;

  // Initialize page table with kernel mappings copied from root
  idle_task->mm_struct.root_satp = init_new_page_table();

  // Initialize VMA list (empty - idle task has no user mappings)
  idle_task->mm_struct.vma_list.next = &idle_task->mm_struct.vma_list;
  idle_task->mm_struct.vma_list.prev = &idle_task->mm_struct.vma_list;

  // Allocate kernel stack
  allocate_kernel_stack(idle_task);

  // Set return address to idle_loop (kernel function, NOT fresh_task_jump)
  // When switch_to returns to idle, it will jump directly to idle_loop
  idle_task->kernel_context.ra = (uint64_t)idle_loop;

  // Don't add to task_list - idle is not a schedulable task
  // Don't add to scheduler lists - idle is the fallback, not scheduled normally
}

// Populates the init_task
void create_init_process() {
  init_task_system();  // Initialize task_list with virtual addresses
  init_task = task_init();
  load_elf(init_task , "/bin/init");
  list_append(&task_list, &init_task->task_list);

  // Set current_task and update tp register
  set_current_task(init_task);
  init_task->state = TASK_RUNNING;
}

// Deprecated, used for testing
void create_second_task() {
  struct task_t *task2 = task_init();
  task2->pid = 2;  // Different PID from init (which is 0)
  load_elf(task2, "/bin/init2");
  list_append(&task_list, &task2->task_list);
  // State is already TASK_READY from task_init()
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

  list_append(&mm_struct->vma_list, &new_vma->sibling_vma);

  // Eagerly load the pages into the page table from the file
  if (eager) {
    size_t file_offset = offset_aligned;
    for (size_t va = vaddr_aligned; va < vaddr_end; va += DEFAULT_PAGE_SIZE) {
      void *phys_page = vfs_get_page(vnode, file_offset);

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
      void *phys_page = get_page(false);

      // Zero the page for security (prevent information leakage)
      void *virt_page = PHYS_TO_VIRT(phys_page);
      memset(virt_page, 0, DEFAULT_PAGE_SIZE);

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

uint64_t fork_off() {
  // Should create a complete copy of the address space of the current task
  // For now we will manually copy over everything on this call  TODO: add copy on write
  struct task_t *new_task = task_t_alloc();
  new_task->ppid = current_task->pid;
  new_task->pid = ++latest_pid;
  new_task->uid = current_task->uid;

  new_task->mm_struct.root_satp = init_new_page_table();
  new_task->mm_struct.vma_list.next = &new_task->mm_struct.vma_list;
  new_task->mm_struct.vma_list.prev = &new_task->mm_struct.vma_list;

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
  list_append(&task_list, &new_task->task_list);
  list_append(scheduler.active_list, &new_task->schedule_list);

  return new_task->pid;
}
