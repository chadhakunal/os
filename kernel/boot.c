#include "platform.h"
#include "memory.h"

void kmain(void* dtb);

void boot(void) {
    // Clear BSS section
    extern char _bss_start[], _bss_end[];
    for (char *p = _bss_start; p < _bss_end; p++)
        *p = 0;
    
    // Get DTB address from a1 register (set by bootloader)
    void *dtb;
    asm ("mv %0, a1" : "=r" (dtb));
    
    // Boot sequence
    if (platform_init(dtb) != 0) {
        return;
    }
    memory_init();
    kmain(dtb);
}
