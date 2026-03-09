// OS Kernel v1.0.0

#include "types.h"
#include "platform.h"
#include "kernel/memory.h"
#include "lib/printk/printk.h"

void kmain(void* dtb_ptr) {
    printk("Kernel Started...\n");
    print_memory();
    while(1) {
        __asm__ volatile("wfe");
    }
}
