#include "platform.h"
#include "memory.h"
#include "drivers/uart.h"

void kmain(void* dtb);

void boot(void) {
    // Clear BSS section
    extern char _bss_start[], _bss_end[];
    for (char *p = _bss_start; p < _bss_end; p++)
        *p = 0;
    
    // Get DTB address from a1 register (set by bootloader)
    void *dtb;
    asm ("mv %0, a1" : "=r" (dtb));
    
    // Early debug: try printing to UART
    uart_putc('B');
    uart_putc('O');
    uart_putc('O');
    uart_putc('T');
    uart_putc('\n');
    
    // Boot sequence
    if (platform_init(dtb) != 0) {
        uart_println("platform_init failed");
        return;
    }
    uart_println("platform_init ok");
    memory_init();
    uart_println("memory_init ok");
    kmain(dtb);
}
