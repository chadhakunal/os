#include "platform.h"
#include "kernel/drivers/uart.h"
#include "string.h"
#include "memory.h"
#include <stddef.h>

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

#define DEBUG_DTB 1
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

struct dtb_property {
    char name[MAX_STR_LEN];
    uint32_t len;
    uint8_t value[256];
};

struct dtb_node {
    char name[MAX_STR_LEN];
    struct dtb_property props[MAX_PROPS_PER_NODE];
    int prop_count;
    int parent_idx;
    int child_idx[32];
    int child_count;
};

struct dtb_tree {
    struct dtb_node nodes[MAX_DTB_NODES];
    int node_count;
};

void uart_print_tabs(int n) {
  for (int i = 0; i < n; i++) {
    uart_print("\t");
  }
}

struct dtb_tree global_dtb = {0};
volatile struct platform_info platform = {0};

static void dtb_memory_init(void *dtb_ptr) {
  // First iterate through dtb to find memory 
  struct fdt_header* hdr = (struct fdt_header*)dtb_ptr;
  
  uint32_t magic = __builtin_bswap32(hdr->magic);
  if (magic != EXPECTED_MAGIC) {
      uart_print("DTB Magic mismatch!\n");
      return;
  }

  uint32_t off_dt_struct = __builtin_bswap32(hdr->off_dt_struct);
  uint32_t size_dt_struct = __builtin_bswap32(hdr->size_dt_struct);
  uint32_t off_dt_strings = __builtin_bswap32(hdr->off_dt_strings);

  uint8_t* base = (uint8_t*)dtb_ptr;
  uint8_t* struct_base = base + off_dt_struct;
  uint8_t* strings_base = base + off_dt_strings;

  uint32_t  i = 0;
  bool in_memory_node = false;

  while (i < size_dt_struct) {
    uint32_t token = __builtin_bswap32(*(uint32_t*)(struct_base + i));
    if (token == FDT_BEGIN_NODE) {
      i += 4;
      // Node name starts right after token, read until null terminator
      char* name_ptr = (char*)(struct_base + i);
      int name_len = 0;
      while (struct_base[i] != '\0' && name_len < MAX_STR_LEN - 1) {
        name_len++;
        i++;
      }
      i++;  // skip null terminator
      
      // Align to 4-byte boundary
      i = (i + 3) & ~3;
      if (strneq_prefix(name_ptr, "memory", 6)) {
        uart_print("Found memory node\n");
        in_memory_node = true;
      }

    } else if (token == FDT_END_NODE) {
      in_memory_node = false;
      // uart_print("end node ...\n");
      i += 4;
    } else if (token == FDT_PROP) {

      //uart_print("prop node ...\n");
      i += 4;

      uint32_t len = __builtin_bswap32(*(uint32_t*)(struct_base + i));
      i += 4;

      uint32_t name_off = __builtin_bswap32(*(uint32_t*)(struct_base + i));
      i += 4;
      char* name_ptr = (char*)(strings_base + name_off);
      if (in_memory_node && strneq_prefix(name_ptr, "reg", 3)) {
        uart_print("Found reg property in memory node\n");
        if (len != 16) {
          uart_print("dtb memory node reg property data is not 16; abort\n");
        }
        uint32_t addr_hi = __builtin_bswap32(*(uint32_t*)(struct_base + i));
        i += 4;
        uint32_t addr_lo = __builtin_bswap32(*(uint32_t*)(struct_base + i));
        i += 4;
        uint32_t size_hi = __builtin_bswap32(*(uint32_t*)(struct_base + i));
        i += 4;
        uint32_t size_lo = __builtin_bswap32(*(uint32_t*)(struct_base + i));
        i += 4;

        uint64_t base = ((uint64_t)addr_hi << 32) | addr_lo;
        uint64_t size = ((uint64_t)size_hi << 32) | size_lo;
        platform.ram.base = base;
        platform.ram.size = size;
        asm volatile("" ::: "memory");
        volatile uint64_t test = platform.ram.base;
        uart_print("Test read: 0x");
        uart_print_hex(test);
        uart_print("\n");
        return;
      } else {
        i += len;
      }
      // Align to 4-byte boundary
      i = (i + 3) & ~3;
    } else if (token == FDT_NOP) {
      //uart_print("nop ...\n");
      i += 4;
    } else if (token == FDT_END) {
      //uart_print("end ...\n");
      break;
    } else {
      i += 4;
    }
  }
}

uint32_t platform_init(void* dtb) {
    uart_print("Platform Init...\n");

    struct fdt_header* hdr = (struct fdt_header*)dtb;

    uint32_t magic = __builtin_bswap32(hdr->magic);
    
    uint32_t off_dt_struct = __builtin_bswap32(hdr->off_dt_struct);
    uint32_t size_dt_struct = __builtin_bswap32(hdr->size_dt_struct);

    uint32_t off_dt_strings = __builtin_bswap32(hdr->off_dt_strings);
    uint32_t size_dt_strings = __builtin_bswap32(hdr->size_dt_strings);


    #ifdef DEBUG_DTB
    uart_print("DTB Address: ");
    uart_print_hex((uint64_t)dtb);

    uart_print("Magic: ");
    uart_print_hex((uint64_t)magic);

    uart_print("Version: ");
    uart_print_int(__builtin_bswap32(hdr->version));

    uart_print("Last Comp Version: ");
    uart_print_int(__builtin_bswap32(hdr->last_comp_version));
    
    uart_print("Boot CPU ID Phys: ");
    uart_print_int(__builtin_bswap32(hdr->boot_cpuid_phys));
    
    uart_print("off_mem_rsvmap: ");
    uart_print_hex((uint64_t)__builtin_bswap32(hdr->off_mem_rsvmap));
    uart_print("off_dt_struct: ");
    uart_print_hex((uint64_t)off_dt_struct);
    uart_print("size_dt_struct: ");
    uart_print_hex((uint64_t)size_dt_struct);
    uart_print("off_dt_strings: ");
    uart_print_hex((uint64_t)off_dt_strings);
    uart_print("size_dt_strings: ");
    uart_print_hex((uint64_t)size_dt_strings);
    #endif

    if(magic == EXPECTED_MAGIC) {
        uart_print("DTB Magic Matched...\n");
    } else {
        uart_print("DTB Magic Did Not Match...\n");
        return -1;
    }

    // Parse DTB into tree structure
    // struct dtb_tree* tree = dtb_parse(dtb);
    dtb_memory_init(dtb);
    uart_print("memory base: ");
    uart_print_hex(platform.ram.base);
    uart_print("\n");
    uart_print("memory size: ");
    uart_print_hex(platform.ram.size);
    uart_print("\n");
    // if (!tree) {
    //     uart_print("Failed to parse DTB\n");
    //     return -1;
    // }

    #ifdef DEBUG_DTB
    //dtb_print_tree(tree);
    #endif

    return 0;
}
