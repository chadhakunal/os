#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "lib/pool_allocator.h"

enum Elf_types {
  ET_EXEC = 2,  // Executable file
  ET_DYN = 3    // Shared object / PIE
};

struct elf_program_entry {
  uint64_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
};

struct elf_file {
  uint64_t e_phoff;
  uint64_t e_phnum;
  uint64_t e_entry;
  enum Elf_types e_type;
  struct elf_program_entry *entries;
};

struct elf_file *parse_elf_file(void *elf_file_start);

DEFINE_POOL(elf_file, struct elf_file)
DEFINE_POOL(elf_program_entry, struct elf_program_entry)

#endif
