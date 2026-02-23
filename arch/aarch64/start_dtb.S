.section .text
.global _start

_start:
    ldr x0, =stack_top
    mov sp, x0

    ldr x0, =0x41000000
    bl boot_entry

1:
    wfe
    b 1b

.section .bss
.align 12
stack:
    .skip 4096

stack_top:
