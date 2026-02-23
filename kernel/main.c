// OS Kernel v1.0.0

#include "types.h"
#include "platform.h"
#include "kernel/drivers/uart.h"

void kmain(void* dtb_ptr) {
    uart_print("Kernel Started...\n");

    while(1) {
        __asm__ volatile("wfe");
    }
}
