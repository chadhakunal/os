// OS Kernel v1.0.0

#include "types.h"
#include "platform.h"
#include "memory.h"
#include "printk.h"
#include "page.h"

void kmain(void* dtb_ptr) {
    printk("Kernel Started...\n");
    print_memory();
    struct pages_metadata_struct *page_table_start = init_paging();
    print_pages_metadata();
    while(1) {
        __asm__ volatile("wfe");
    }
}
