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

    # Call trap_return (a0 already has trap_frame pointer)
    # This will never return
    call trap_return

    # Should never reach here
    j .
