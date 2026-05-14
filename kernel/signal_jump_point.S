.section .text
.global __signal_jump_point_start
.global __signal_jump_point_end
.align 4

__signal_jump_point_start:
    li a7, 139          # SYS_rt_sigreturn
    ecall
    j .                 # should never reach here
__signal_jump_point_end:
