#ifndef ELF_PARSER_H
#define ELF_PARSER_H

typedef struct elf_header {
    unsigned char e_ident[16]; // magic, class, etc.
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;   // entry point
    uint64_t e_phoff;   // program header offset
    uint64_t e_shoff;   // section header offset (ignore)
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize; // size of each program header
    uint16_t e_phnum;     // number of program headers
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf_header_t;


typedef struct elf {

} elf_t;

elf_t parse_elf(void* elf_addr);

#endif
