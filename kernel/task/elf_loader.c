#include "kernel/task/elf_loader.h"
#include "lib/printk/printk.h"

struct elf_file *parse_elf_file(void *elf_file_start) {
  struct elf_file *parsed_elf_file = elf_file_alloc();
  uint8_t *elf_base = (uint8_t *)elf_file_start;

  parsed_elf_file->e_type = *((uint16_t *)(elf_base + 0x10));
  parsed_elf_file->e_entry = *((uint64_t *)(elf_base + 0x18));
  parsed_elf_file->e_phoff = *((uint64_t *)(elf_base + 0x20));
  parsed_elf_file->e_phnum = *((uint16_t *)(elf_base + 0x38));

  printk("\n=== ELF File Parsed ===\n");
  printk("e_type:   0x%x\n", parsed_elf_file->e_type);
  printk("e_entry:  0x%llx\n", parsed_elf_file->e_entry);
  printk("e_phoff:  0x%llx\n", parsed_elf_file->e_phoff);
  printk("e_phnum:  %llu\n", parsed_elf_file->e_phnum);

  // We should go through each program entry
  // for each entry
  // allocate ceil(memsz/Page_size) pages, these should be in user space! 
  // we then read
  

  return parsed_elf_file;
}

void load_elf(struct task_t *task, const char *path) {
  struct dentry_t *dentry;
  int64_t ret = vfs_resolve_path(path, &dentry);
  if (ret != 0) {
    panic("load_elf: vfs_resolve_path returned with %lld\n", ret);
    return;
  }

  // Go through 
  struct Elf64_Ehdr header;
  vfs_vnode_read(dentry->vnode, &header, sizeof(header), 0);
}
