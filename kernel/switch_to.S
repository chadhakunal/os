.section .text
.global switch_to
.align 4

# switch_to(struct task_t *me, struct task_t *next)
# a0 = me (current task being switched out)
# a1 = next (next task being switched to)
#
# Context switch flow:
#   1. Save current task's kernel context (ra, sp, s0-s11) to me->kernel_context
#   2. Restore next task's kernel context from next->kernel_context
#   3. Return to next task's saved ra (resumes where next was switched out)
#
# Note: kernel_context is at offset 288 within task_t (sizeof trap_frame)
switch_to:
    # Calculate address of me->kernel_context (me + 288)
    addi t0, a0, 288

    # Save current task's kernel context to me->kernel_context
    sd ra, 0(t0)      # me->kernel_context.ra = ra
    sd sp, 8(t0)      # me->kernel_context.sp = sp
    sd s0, 16(t0)     # me->kernel_context.s[0] = s0
    sd s1, 24(t0)     # me->kernel_context.s[1] = s1
    sd s2, 32(t0)     # me->kernel_context.s[2] = s2
    sd s3, 40(t0)     # me->kernel_context.s[3] = s3
    sd s4, 48(t0)     # me->kernel_context.s[4] = s4
    sd s5, 56(t0)     # me->kernel_context.s[5] = s5
    sd s6, 64(t0)     # me->kernel_context.s[6] = s6
    sd s7, 72(t0)     # me->kernel_context.s[7] = s7
    sd s8, 80(t0)     # me->kernel_context.s[8] = s8
    sd s9, 88(t0)     # me->kernel_context.s[9] = s9
    sd s10, 96(t0)    # me->kernel_context.s[10] = s10
    sd s11, 104(t0)   # me->kernel_context.s[11] = s11

    # Calculate address of next->kernel_context (next + 288)
    addi t1, a1, 288

    # Restore next task's kernel context from next->kernel_context
    ld ra, 0(t1)      # ra = next->kernel_context.ra
    ld sp, 8(t1)      # sp = next->kernel_context.sp (switch stacks!)
    ld s0, 16(t1)     # s0 = next->kernel_context.s[0]
    ld s1, 24(t1)     # s1 = next->kernel_context.s[1]
    ld s2, 32(t1)     # s2 = next->kernel_context.s[2]
    ld s3, 40(t1)     # s3 = next->kernel_context.s[3]
    ld s4, 48(t1)     # s4 = next->kernel_context.s[4]
    ld s5, 56(t1)     # s5 = next->kernel_context.s[5]
    ld s6, 64(t1)     # s6 = next->kernel_context.s[6]
    ld s7, 72(t1)     # s7 = next->kernel_context.s[7]
    ld s8, 80(t1)     # s8 = next->kernel_context.s[8]
    ld s9, 88(t1)     # s9 = next->kernel_context.s[9]
    ld s10, 96(t1)    # s10 = next->kernel_context.s[10]
    ld s11, 104(t1)   # s11 = next->kernel_context.s[11]

    # Return to next task (jumps to address in ra)
    # This will resume next task at the point where it was switched out
    ret
