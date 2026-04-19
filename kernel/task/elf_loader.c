#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "panic.h"

void load_elf(struct task_t *task, const char *path) {
  struct dentry_t *dentry;
  int64_t ret = vfs_resolve_path(path, &dentry);
  if (ret != 0) {
    panic("load_elf: vfs_resolve_path returned with error\n");
    return;
  }

  // Read ELF header
  struct Elf64_Ehdr header;
  vfs_vnode_read(dentry->vnode, &header, sizeof(header), 0);

  // Print ELF header information
  printk("=== ELF Header ===\n");
  printk("Magic:   %02x %02x %02x %02x\n",
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
}
