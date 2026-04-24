#include "kernel/task/task.h"
#include "lib/list.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"
#include "lib/string.h"
#include "kernel/task/elf_loader.h"

// Global task tracking
struct task_t *current_task = NULL;  // Currently running task
struct task_t *init_task = NULL;     // First task (PID 0 or 1)
struct list_node task_list = {       // Global list of all tasks (initialized empty)
  .next = &task_list,
  .prev = &task_list
};

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
  printk("task_init: Starting...\n");
  struct task_t *task = task_t_alloc();
  printk("task_init: task_t_alloc done, task=%p\n", task);
  task->pid = 0;
  task->uid = 0;

  // Initialize page table with kernel mappings copied from root
  task->mm_struct.root_satp = init_new_page_table();

  // Initialize VMA list
  task->mm_struct.vma_list.next = &task->mm_struct.vma_list;
  task->mm_struct.vma_list.prev = &task->mm_struct.vma_list;

  //TODO: Integrate this with the base process
  task->task_list.next = &task->task_list;
  task->task_list.prev = &task->task_list;

  init_files(&(task->file_table));

  // Allocate kernel stack and set SP to TOP (stacks grow down)
  task->kernel_context.stack_start = (uint64_t)PHYS_TO_VIRT(get_page(true));
  task->kernel_context.sp = task->kernel_context.stack_start + 4096;  // Point to top

  return task;
}

// Populates the init_task
void create_init_process() {
  printk("create_init_process: Starting...\n");
  init_task = task_init();
  printk("create_init_process: task_init done, root_satp=%p\n", init_task->mm_struct.root_satp);
  printk("create_init_process: root_satp (virt)=%p\n", PHYS_TO_VIRT(init_task->mm_struct.root_satp));
  load_elf(init_task , "/bin/init");
  printk("create_init_process: load_elf done\n");
  list_append(&task_list, &init_task->task_list);
  current_task = init_task;
  printk("create_init_process: Done\n");
}

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
