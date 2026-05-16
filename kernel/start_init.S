.section .text
.global start_init_task
.align 4

# start_init_task(struct trap_frame *tf, uint64_t kernel_sp)
# a0 = trap_frame pointer
# a1 = kernel stack pointer to switch to
#
# This function switches to the init task's kernel stack
# and then calls trap_return to enter user mode.
# This properly sets up the init task so it can be scheduled later.
start_init_task:
    # Switch to init task's kernel stack
    mv sp, a1

    # Save trap_frame pointer (a0) across calls
    mv s0, a0

    # Log that we reached start_init_task
    la a0, start_init_msg
    call printk

    # Enable interrupts now that we are on the init task's kernel stack
    call enable_interrupts

    la a0, enable_irq_msg
    call printk

    # Restore trap_frame pointer and jump to user mode (never returns)
    mv a0, s0
    call trap_return

    # Should never reach here
    j .

.section .rodata
start_init_msg:
    .string "boot: start_init_task reached\n"
enable_irq_msg:
    .string "boot: interrupts enabled, calling trap_return\n"
