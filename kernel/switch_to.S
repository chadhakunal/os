.section .text
.global switch_to
.align 4

# switch_to(struct task_t *me, struct task_t *next)
# a0 = me (current task being switched out)
# a1 = next (next task being switched to)
#
# Context switch flow:
#   1. Save current task's kernel context (sp, ra, s0-s11) to me->kernel_context
#   2. Restore next task's kernel context from next->kernel_context
#   3. Return to next task's saved ra (resumes where next was switched out)
#
# Note: kernel_context is at offset 0 within task_t
# struct kernel_context_t {
#   uint64_t sp;        // offset 0
#   uint64_t ra;        // offset 8
#   uint64_t s[12];     // offset 16, 24, ..., 104
#   uint64_t stack_start; // offset 112
# };
switch_to:
    # me->kernel_context is at a0 + 0 (just a0)
    # Save current task's kernel context to me->kernel_context
    sd sp, 0(a0)      # me->kernel_context.sp = sp
    sd ra, 8(a0)      # me->kernel_context.ra = ra
    sd s0, 16(a0)     # me->kernel_context.s[0] = s0
    sd s1, 24(a0)     # me->kernel_context.s[1] = s1
    sd s2, 32(a0)     # me->kernel_context.s[2] = s2
    sd s3, 40(a0)     # me->kernel_context.s[3] = s3
    sd s4, 48(a0)     # me->kernel_context.s[4] = s4
    sd s5, 56(a0)     # me->kernel_context.s[5] = s5
    sd s6, 64(a0)     # me->kernel_context.s[6] = s6
    sd s7, 72(a0)     # me->kernel_context.s[7] = s7
    sd s8, 80(a0)     # me->kernel_context.s[8] = s8
    sd s9, 88(a0)     # me->kernel_context.s[9] = s9
    sd s10, 96(a0)    # me->kernel_context.s[10] = s10
    sd s11, 104(a0)   # me->kernel_context.s[11] = s11

    # next->kernel_context is at a1 + 0 (just a1)
    # Restore next task's kernel context from next->kernel_context
    ld sp, 0(a1)      # sp = next->kernel_context.sp (switch stacks!)
    ld ra, 8(a1)      # ra = next->kernel_context.ra
    ld s0, 16(a1)     # s0 = next->kernel_context.s[0]
    ld s1, 24(a1)     # s1 = next->kernel_context.s[1]
    ld s2, 32(a1)     # s2 = next->kernel_context.s[2]
    ld s3, 40(a1)     # s3 = next->kernel_context.s[3]
    ld s4, 48(a1)     # s4 = next->kernel_context.s[4]
    ld s5, 56(a1)     # s5 = next->kernel_context.s[5]
    ld s6, 64(a1)     # s6 = next->kernel_context.s[6]
    ld s7, 72(a1)     # s7 = next->kernel_context.s[7]
    ld s8, 80(a1)     # s8 = next->kernel_context.s[8]
    ld s9, 88(a1)     # s9 = next->kernel_context.s[9]
    ld s10, 96(a1)    # s10 = next->kernel_context.s[10]
    ld s11, 104(a1)   # s11 = next->kernel_context.s[11]

    # Update sscratch with next task's kernel stack top
    # sscratch is used by trap_vector to swap to kernel stack
    # stack_start is at offset 112, we want sp (top) which is at offset 0
    ld t0, 0(a1)      # t0 = next->kernel_context.sp (kernel stack top)
    csrw sscratch, t0 # sscratch = kernel stack top for this task

    # Return to next task (jumps to address in ra)
    # This will resume next task at the point where it was switched out
    ret
