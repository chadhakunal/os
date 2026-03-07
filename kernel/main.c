// OS Kernel v1.0.0

#include "types.h"
#include "platform.h"
#include "kernel/memory.h"
#include "kernel/drivers/uart.h"
#include "page.h"

void kmain(void* dtb_ptr) {
    uart_print("Kernel Started...\n");
    print_memory();
    struct pages_metadata_struct *page_table_start = init_paging();
    print_pages_metadata();

    while(1) {
        __asm__ volatile("wfe");
    }
}
