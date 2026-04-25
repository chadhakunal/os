#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/panic.h"

void load_elf(struct task_t *task, const char *path) {
  struct dentry_t *dentry;
  int32_t ret = vfs_resolve_path(path, &dentry);
  if (ret != 0) {
    panic("load_elf: vfs_resolve_path returned with error\n");
  }

  // Read ELF header
  struct Elf64_Ehdr header;
  ret = vfs_vnode_read(dentry->vnode, &header, sizeof(header), 0);
  if (ret < 0) {
    panic("load_elf: Could not load elf\n");
  }
  task->mm_struct.entry_addr = (void *)header.e_entry;

  // Print ELF header information
  printk("=== ELF Header ===\n");
  printk("Magic:   0x%x 0x%x 0x%x 0x%x\n",
         header.e_ident[EI_MAG0], header.e_ident[EI_MAG1],
         header.e_ident[EI_MAG2], header.e_ident[EI_MAG3]);
  printk("Class:   %d (%s)\n", header.e_ident[EI_CLASS],
         header.e_ident[EI_CLASS] == ELFCLASS64 ? "ELF64" :
         header.e_ident[EI_CLASS] == ELFCLASS32 ? "ELF32" : "Unknown");
  printk("Data:    %d (%s)\n", header.e_ident[EI_DATA],
         header.e_ident[EI_DATA] == ELFDATA2LSB ? "Little Endian" :
         header.e_ident[EI_DATA] == ELFDATA2MSB ? "Big Endian" : "Unknown");
  printk("Type:    %d (%s)\n", header.e_type,
         header.e_type == ET_EXEC ? "EXEC" :
         header.e_type == ET_DYN ? "DYN" : "Other");
  printk("Machine: %d (%s)\n", header.e_machine,
         header.e_machine == EM_RISCV ? "RISC-V" : "Other");
  printk("Entry:   0x%lx\n", header.e_entry);
  printk("Program Headers: offset=0x%lx, count=%d, size=%d\n",
         header.e_phoff, header.e_phnum, header.e_phentsize);
  printk("Section Headers: offset=0x%lx, count=%d, size=%d\n",
         header.e_shoff, header.e_shnum, header.e_shentsize);
  // Read first program header
  struct Elf64_Phdr program_header;
  for (size_t pheader_idx = 0; pheader_idx < header.e_phnum; pheader_idx ++) {
    vfs_vnode_read(dentry->vnode, &program_header, sizeof(program_header), (size_t) (header.e_phoff + sizeof(program_header) * pheader_idx));

    // Print program header information
    printk("\n=== Program Header %d ===\n", pheader_idx);
    printk("Type:    %d (%s)\n", program_header.p_type,
          program_header.p_type == PT_LOAD ? "LOAD" :
          program_header.p_type == PT_DYNAMIC ? "DYNAMIC" :
          program_header.p_type == PT_INTERP ? "INTERP" :
          program_header.p_type == PT_PHDR ? "PHDR" : "Other");
    printk("Offset:  0x%lx\n", program_header.p_offset);
    printk("VirtAddr: 0x%lx\n", program_header.p_vaddr);
    printk("PhysAddr: 0x%lx\n", program_header.p_paddr);
    printk("FileSiz: 0x%lx (%lu bytes)\n", program_header.p_filesz, program_header.p_filesz);
    printk("MemSiz:  0x%lx (%lu bytes)\n", program_header.p_memsz, program_header.p_memsz);
    printk("Flags:   0x%x (", program_header.p_flags);
    if (program_header.p_flags & PF_R) printk("R");
    if (program_header.p_flags & PF_W) printk("W");
    if (program_header.p_flags & PF_X) printk("X");
    printk(")\n");
    printk("Align:   0x%lx\n", program_header.p_align);

    uint64_t vm_flags = 0;
    if (program_header.p_type == PT_LOAD) {
      if (program_header.p_flags & PF_R) vm_flags |= VM_READ;
      if (program_header.p_flags & PF_W) vm_flags |= VM_WRITE;
      if (program_header.p_flags & PF_X) vm_flags |= VM_EXEC;

      file_backed_memory_map(&task->mm_struct, program_header.p_vaddr, dentry->vnode, program_header.p_offset, program_header.p_memsz, vm_flags, true);
    }
  }

  // Set up stack (read/write, no execute for security)
  // 4 pages = 16KB, stack grows down from DEFAULT_STACK_TOP
  anon_memory_map(&task->mm_struct, DEFAULT_STACK_START, DEFAULT_STACK_SIZE, VM_READ | VM_WRITE, true);

  // Initialize trap frame - zero everything first
  memset(&task->tf, 0, sizeof(task->tf));

  // Set up user mode entry
  task->tf.sepc = header.e_entry;
  task->tf.sp = DEFAULT_STACK_TOP;  // 16-byte aligned (RISC-V ABI requirement)
  task->tf.sstatus = SSTATUS_SPIE | SSTATUS_UXL_64;
}
