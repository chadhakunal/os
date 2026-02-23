#include "platform.h"
#include "kernel/drivers/uart.h"

#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

#define DEBUG_DTB 1
#define EXPECTED_MAGIC 0xD00DFEED

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

    uint8_t* base = (uint8_t*)dtb;
    uint8_t* struct_base = (uint8_t*)(base + off_dt_struct);
    uint8_t* strings_base = (uint8_t*)(base + off_dt_strings);

    int i = 0;
    while (i < size_dt_struct) {
        uint32_t d = __builtin_bswap32(*(uint32_t*)(struct_base + i));

        if (d == FDT_BEGIN_NODE) {
            uart_print("BEGIN NODE\n\t");
            i += 4;

            char name_buffer[64];
            int j = 0;

            while (struct_base[i] != '\0') {
                if (j < 63) {
                    name_buffer[j++] = struct_base[i];
                }
                i++;
            }

            name_buffer[j] = '\0';
            i++;  // skip null terminator

            // Align to 4-byte boundary
            i = (i + 3) & ~3;

            uart_print(name_buffer);
            uart_print("\n");
        } else if (d == FDT_END_NODE) {
            uart_print("END NODE\n");
            i += 4;
        } else if (d == FDT_PROP) {
            uart_print("PROP\n");
            i += 4;

            uint32_t len = __builtin_bswap32(*(uint32_t*)(struct_base + i));
            i += 4;

            uint32_t name_off = __builtin_bswap32(*(uint32_t*)(struct_base + i));
            i += 4;

            // Skip property value
            i += len;

            // Align
            i = (i + 3) & ~3;
        } else if (d == FDT_NOP) {
            i += 4;
        } else if (d == FDT_END) {
            uart_print("END\n");
            break;
        } else {
            uart_print("UNKNOWN TOKEN\n");
            i += 4;
        }
    }


    // for(int i = 0; i < 25; i++) {
    //     uint32_t d = __builtin_bswap32(struct_base[i]);
    //     if(d == FDT_BEGIN_NODE) {
    //         uart_print("BEGIN NODE\n");
    //     } else if(d == FDT_END_NODE) {
    //         uart_print("END NODE\n");
    //     } else if(d == FDT_PROP) {
    //         uart_print("PROP\n");
    //     } else if(d == FDT_NOP) {
    //         uart_print("NOP\n");
    //     } else if(d == FDT_END) {
    //         uart_print("END\n");
    //     } else {
    //         uart_print_hex_32(d);
    //     }
    // }


    // uint8_t* strings_base = (uint8_t*)dtb + off_dt_strings;
    


    // uart_print("Strings Data: \n");
    // int i = 0; 
    // while(i < size_dt_strings) {
    //     char c = strings_base[i];

    //     if (c == '\0') {
    //         uart_putc('\n');
    //     } else {
    //         uart_putc(c);
    //     }

    //     i+=1;
    // }
    // uart_print("\n");



    // uart_print("Struct Data: \n");
    // i = 0; 
    // while(i < size_dt_struct) {
    //     uart_putc(struct_base[i]);
    //     i+=1;
    // }
    // uart_print("\n");

    return 0;
}
