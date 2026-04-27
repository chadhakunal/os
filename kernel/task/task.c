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
struct task_t *init_task = NULL;     // First task (PID 0 or 1)
struct list_node task_list;          // Global list of all tasks

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

struct task_t *task_init() {
  struct task_t *task = task_t_alloc();
  task->pid = 0;
  task->uid = 0;
  task->state = TASK_READY;

  // Initialize page table with kernel mappings copied from root
  task->mm_struct.root_satp = init_new_page_table();

  // Initialize VMA list (empty circular list)
  task->mm_struct.vma_list.next = &task->mm_struct.vma_list;
  task->mm_struct.vma_list.prev = &task->mm_struct.vma_list;

  init_files(&(task->file_table));

  // Allocate kernel stack (8KB = 2 pages) mapped at fixed virtual address
  // All processes have their kernel stack at the same VA, but different physical pages
  // Allocate 2 physical pages
  void *phys_page1 = get_page(true);
  void *phys_page2 = get_page(true);
  printk("Mapping pages for kernel stack\n");
  // Map them to the kernel stack virtual address in this task's page table
  // KERNEL_STACK_VIRTUAL_BASE is the same for all tasks
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE,
           (uint64_t)phys_page1, PTE_VALID | PTE_R | PTE_W);
  map_page(task->mm_struct.root_satp, KERNEL_STACK_VIRTUAL_BASE + 4096,
           (uint64_t)phys_page2, PTE_VALID | PTE_R | PTE_W);

  // Set stack_start to the virtual address (not physical)
  task->kernel_context.stack_start = KERNEL_STACK_VIRTUAL_BASE;
  // SP points to TOP of stack (stacks grow down)
  task->kernel_context.sp = KERNEL_STACK_VIRTUAL_BASE + KERNEL_STACK_SIZE;

  // Set return address for when this task is first scheduled
  // switch_to() will load this ra and ret to it
  // task_entry doesn't try to return - it directly calls trap_return()
  task->kernel_context.ra = (uint64_t)task_entry;

  return task;
}

// Populates the init_task
void create_init_process() {
  init_task_system();  // Initialize task_list with virtual addresses
  init_task = task_init();
  printk("Loading elf for /bin/init\n");
  load_elf(init_task , "/bin/init");
  list_append(&task_list, &init_task->task_list);

  // Set current_task and update tp register
  set_current_task(init_task);
  init_task->state = TASK_RUNNING;
}

void create_second_task() {
  struct task_t *task2 = task_init();
  task2->pid = 1;  // Different PID from init (which is 0)
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

      map_page(PHYS_TO_VIRT(mm_struct->root_satp), va, (uint64_t)phys_page, pte_flags);
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

      map_page(PHYS_TO_VIRT(mm_struct->root_satp), va, (uint64_t)phys_page, pte_flags);
    }
  }

  return 0;
}
