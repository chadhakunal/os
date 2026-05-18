.section .text
.global switch_to
.align 4

# switch_to(struct task_t *me, struct task_t *next)
# a0 = me (current task being switched out)
# a1 = next (next task being switched to)
#
# Context switch flow:
#   1. Save current task's kernel context (sp, ra, s0-s11) to me->kernel_context
#   2. Switch to next task's page table (so next's kernel stack is accessible)
#   3. Restore next task's kernel context from next->kernel_context
#   4. Update sscratch to next task's kernel stack top
#   5. Return to next task's saved ra (resumes where next was switched out)
#
# Note: kernel_context is at offset 0 within task_t
# struct kernel_context_t {
#   uint64_t sp;        // offset 0
#   uint64_t ra;        // offset 8
#   uint64_t s[12];     // offset 16, 24, ..., 104
#   uint64_t stack_start; // offset 112
# };
#
# task_t layout:
#   kernel_context at offset 0 (120 bytes)
#   tf at offset 120 (288 bytes)
#   pid at offset 408 (8 bytes)
#   uid at offset 416 (4 bytes + 4 padding)
#   mm_struct at offset 424
#     root_satp at offset 424 (first field of mm_struct)
switch_to:
    # Disable supervisor interrupts for the duration of the context switch.
    # A timer interrupt mid-switch (after satp changes but before sp is restored)
    # would land on the wrong task's kernel stack and corrupt it.
    # Save SIE state in t2, then clear SIE.
    csrr t2, sstatus
    csrci sstatus, 2        # clear SIE (bit 1)

    # me->kernel_context is at a0 + 0 (just a0)
    # Check if 'me' is idle task (PID 0)
    # PID is at offset 408 in task_t
    ld t0, 408(a0)    # t0 = me->pid
    bnez t0, 1f       # If PID != 0, jump to normal save

    # Switching FROM idle - don't save its context!
    # Idle's kernel_context is set once at creation and must not be overwritten
    j 2f              # Jump to page table switch

1:  # Normal task - save kernel context to me->kernel_context
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

2:  # Switch page tables to next task's address space
    # IMPORTANT: Do this BEFORE restoring sp, because sp will point to next's stack
    # which is only valid in next's page table
    #
    # next->mm_struct.root_satp is at offset 120 + 0 (kernel_context=120, mm_struct at offset 0 after kernel_context)
    # Actually, need to find the correct offset - let's use a register and calculate
    # For now, we'll call switch_to_page_table from C before this
    #
    # Save a1 (next task pointer) because we'll need it after the call
    mv s10, a1        # s10 = next (temporarily use s10, will restore it later)

    # Load next->mm_struct.root_satp
    # task_t layout: kernel_context (120 bytes) + tf (288 bytes) + pid (8) + uid (4+4 padding) = 424
    # mm_struct is at offset 424
    # root_satp is the first field in mm_struct
    ld a0, 424(s10)   # a0 = next->mm_struct.root_satp

    # Switch to next task's page table
    # satp format: MODE (bits 60-63) | ASID (bits 44-59) | PPN (bits 0-43)
    # For Sv39: MODE = 8
    li t0, 8
    slli t0, t0, 60   # t0 = MODE field (8 << 60)
    srli a0, a0, 12   # a0 = PPN (physical page number = pa >> 12)
    or t0, t0, a0     # t0 = satp value
    csrw satp, t0

    # Fence to flush TLB
    sfence.vma zero, zero

    # Restore a1 from s10
    mv a1, s10

    # Now restore next task's kernel context from next->kernel_context
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

    # Set sscratch to 0 to indicate we are in kernel mode.
    # trap_vector checks: if sscratch==0 after swap → came from kernel.
    # trap_return will set sscratch to the real kernel stack top before
    # returning to user mode, so we don't need to do it here.
    csrw sscratch, zero

    # Restore SIE to whatever the caller had (t2 holds the saved sstatus).
    # Only restore SIE bit (bit 1); leave all other bits as-is from new context.
    andi t2, t2, 2          # isolate SIE bit from saved sstatus
    csrs sstatus, t2        # set SIE if it was set before

    # Return to next task (jumps to address in ra)
    # This will resume next task at the point where it was switched out
    ret
