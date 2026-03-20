// OS Kernel v1.0.0

#include "types.h"
#include "platform.h"
#include "kernel/memory.h"
#include "lib/printk/printk.h"
#include "page.h"

extern void* create_initial_page_table();
extern void enable_virtual_memory(uint64_t addr);

/*
    TODO: NEON/FP Unit needs to be enabled 
          It is currently disabled in the HW. Added -mgeneral-regs-only 
          to the makefile so that the compiler doesn't use those units)
*/

void kmain(void* dtb_ptr) {
    printk("Kernel Started...\n");
    memory_init();
    
    print_memory();
    struct pages_metadata_struct *page_table_start = init_paging();
    print_pages_metadata();

    void* root_page_table_addr = create_initial_page_table();
    enable_virtual_memory((uint64_t)root_page_table_addr);
    
    while(1) {
        __asm__ volatile("wfe");
    }
}
