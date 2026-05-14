.section .text.signal_jump_point
.global __signal_jump_point_start
.global __signal_jump_point_end
.align 4

__signal_jump_point_start:
    # At this point, the signal handler has returned
    # We need to call rt_sigreturn syscall to restore context

    # NOTE: Cannot add debug logs here since we're in user space
    # and don't have access to kernel functions

    # Load syscall number for rt_sigreturn into a7
    li a7, 139 # SYS_rt_sigreturn
    ecall

    # Should never reach here - loop forever if we do
    j .

__signal_jump_point_end:
