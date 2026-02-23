#include "platform.h"

void kmain(void* dtb);

void boot_entry(void* dtb) {
    if(platform_init(dtb) != 0) {
        return;
    }
    
    kmain(dtb);
}
