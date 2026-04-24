#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "types.h"
#include "kernel/task/task.h"

/* User stack: place it high in user space, just below kernel boundary (0x80000000) */
#define DEFAULT_STACK_PAGES 4
#define DEFAULT_STACK_SIZE  (DEFAULT_STACK_PAGES * 4096)  /* 16KB = 4 pages */
#define DEFAULT_STACK_TOP   0x80000000  /* Top of user space */
#define DEFAULT_STACK_START (DEFAULT_STACK_TOP - DEFAULT_STACK_SIZE)  /* 0x7FFFC000 */

/* ELF magic number */
#define ELF_MAGIC 0x464C457FU  /* "\x7fELF" in little endian */

/* e_ident[] indexes */
#define EI_MAG0       0  /* 0x7F */
#define EI_MAG1       1  /* 'E' */
#define EI_MAG2       2  /* 'L' */
#define EI_MAG3       3  /* 'F' */
#define EI_CLASS      4  /* File class */
#define EI_DATA       5  /* Data encoding */
#define EI_VERSION    6  /* File version */
#define EI_OSABI      7  /* OS/ABI identification */
#define EI_ABIVERSION 8  /* ABI version */
#define EI_NIDENT     16 /* Size of e_ident[] */

/* EI_CLASS */
#define ELFCLASS32 1  /* 32-bit objects */
#define ELFCLASS64 2  /* 64-bit objects */

/* EI_DATA */
#define ELFDATA2LSB 1  /* Little endian */
#define ELFDATA2MSB 2  /* Big endian */

/* e_type */
#define ET_NONE   0  /* No file type */
#define ET_REL    1  /* Relocatable file */
#define ET_EXEC   2  /* Executable file */
#define ET_DYN    3  /* Shared object file */
#define ET_CORE   4  /* Core file */

/* e_machine */
#define EM_RISCV  243  /* RISC-V */

/* p_type */
#define PT_NULL    0  /* Unused entry */
#define PT_LOAD    1  /* Loadable segment */
#define PT_DYNAMIC 2  /* Dynamic linking information */
#define PT_INTERP  3  /* Interpreter path */
#define PT_NOTE    4  /* Auxiliary information */
#define PT_SHLIB   5  /* Reserved */
#define PT_PHDR    6  /* Program header table */
#define PT_TLS     7  /* Thread-Local Storage */

/* p_flags */
#define PF_X 0x1  /* Execute */
#define PF_W 0x2  /* Write */
#define PF_R 0x4  /* Read */

/* ELF64 Header */
struct Elf64_Ehdr {
  uint8_t  e_ident[EI_NIDENT];  /* ELF identification */
  uint16_t e_type;               /* Object file type */
  uint16_t e_machine;            /* Machine type */
  uint32_t e_version;            /* Object file version */
  uint64_t e_entry;              /* Entry point address */
  uint64_t e_phoff;              /* Program header offset */
  uint64_t e_shoff;              /* Section header offset */
  uint32_t e_flags;              /* Processor-specific flags */
  uint16_t e_ehsize;             /* ELF header size */
  uint16_t e_phentsize;          /* Size of program header entry */
  uint16_t e_phnum;              /* Number of program header entries */
  uint16_t e_shentsize;          /* Size of section header entry */
  uint16_t e_shnum;              /* Number of section header entries */
  uint16_t e_shstrndx;           /* Section name string table index */
};

/* ELF64 Program Header */
struct Elf64_Phdr {
  uint32_t p_type;    /* Segment type */
  uint32_t p_flags;   /* Segment flags */
  uint64_t p_offset;  /* Segment file offset */
  uint64_t p_vaddr;   /* Segment virtual address */
  uint64_t p_paddr;   /* Segment physical address */
  uint64_t p_filesz;  /* Segment size in file */
  uint64_t p_memsz;   /* Segment size in memory */
  uint64_t p_align;   /* Segment alignment */
};

/* ELF64 Section Header */
struct Elf64_Shdr {
  uint32_t sh_name;       /* Section name (string table index) */
  uint32_t sh_type;       /* Section type */
  uint64_t sh_flags;      /* Section flags */
  uint64_t sh_addr;       /* Section virtual address */
  uint64_t sh_offset;     /* Section file offset */
  uint64_t sh_size;       /* Section size in bytes */
  uint32_t sh_link;       /* Link to another section */
  uint32_t sh_info;       /* Additional section information */
  uint64_t sh_addralign;  /* Section alignment */
  uint64_t sh_entsize;    /* Entry size if section holds table */
};

void load_elf(struct task_t *task, const char *path);

#endif
