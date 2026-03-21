.section .text
.global _start

.extern __bss_start
.extern __bss_end
.extern boot_entry

_start:
    ldr     x0, =stack_top
    mov     sp, x0

    ldr     x1, =__bss_start
    ldr     x2, =__bss_end

clear_bss:
    cmp     x1, x2
    b.ge    bss_done
    str     xzr, [x1], #8
    b       clear_bss

bss_done:

    ldr     x0, =0x41000000
    bl      boot_entry

hang:
    wfi
    b       hang

.section .bss
.align 12                  // 4096-byte alignment
stack:
    .skip   16384          // 16 KB stack
stack_top:
