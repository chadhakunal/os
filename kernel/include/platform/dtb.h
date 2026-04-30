#ifndef PLATFORM_DTB_H
#define PLATFORM_DTB_H

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

#define EXPECTED_MAGIC 0xD00DFEED

#define MAX_DTB_NODES 24
#define MAX_PROPS_PER_NODE 12
#define MAX_STR_LEN 256

struct fdt_header {
    uint32_t magic;              // 0xd00dfeed
    uint32_t totalsize;          // total DTB size in bytes
    uint32_t off_dt_struct;      // offset to structure block
    uint32_t off_dt_strings;     // offset to strings block
    uint32_t off_mem_rsvmap;     // offset to memory reservation block
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;    // size of strings block
    uint32_t size_dt_struct;     // size of structure block
};

#endif
