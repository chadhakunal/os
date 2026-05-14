.section .text
.global trap_vector
.align 4

trap_vector:
  # tp register points to current_task (task_t*)
  # task_t.kernel_context is at offset 0 (120 bytes)
  # task_t.tf is at offset 120 (288 bytes)

  # Swap sp with sscratch to determine where we came from
  csrrw sp, sscratch, sp
  # If sp == 0 after swap, we came from kernel mode
  # If sp != 0 after swap, we came from user mode (sp now has kernel stack)
  beqz sp, trap_from_kernel

trap_from_user:
  # Coming from user mode:
  # sp = kernel stack, sscratch = user stack pointer

  # Save current kernel sp to kernel_context.sp (offset 0)
  # This ensures trap_return can restore sscratch correctly even without a context switch
  sd sp, 0(tp)

  # Save all general purpose registers to current_task->tf (at tp + 120)
  sd ra, 120(tp)
  # Save user sp (currently in sscratch)
  csrr t0, sscratch
  sd t0, 128(tp)
  sd gp, 136(tp)
  # Note: We don't save tp itself - it always points to current_task
  sd t0, 152(tp)     # Save t0 (we're using it as scratch)
  sd t1, 160(tp)
  sd t2, 168(tp)
  sd s0, 176(tp)
  sd s1, 184(tp)
  sd a0, 192(tp)
  sd a1, 200(tp)
  sd a2, 208(tp)
  sd a3, 216(tp)
  sd a4, 224(tp)
  sd a5, 232(tp)
  sd a6, 240(tp)
  sd a7, 248(tp)
  sd s2, 256(tp)
  sd s3, 264(tp)
  sd s4, 272(tp)
  sd s5, 280(tp)
  sd s6, 288(tp)
  sd s7, 296(tp)
  sd s8, 304(tp)
  sd s9, 312(tp)
  sd s10, 320(tp)
  sd s11, 328(tp)
  sd t3, 336(tp)
  sd t4, 344(tp)
  sd t5, 352(tp)
  sd t6, 360(tp)

  # Save CSRs
  csrr t0, sepc
  sd t0, 368(tp)
  csrr t0, sstatus
  sd t0, 376(tp)
  csrr t0, stval
  sd t0, 384(tp)
  csrr t0, scause
  sd t0, 392(tp)

  # Mark that we're now in kernel mode (for nested traps)
  # Set sscratch to 0 so nested traps know we're in kernel
  csrw sscratch, zero

  # Re-enable interrupts now that context is saved
  # Hardware automatically disables interrupts (clears SIE) on trap entry
  # We re-enable them so timer/UART interrupts can be handled during trap processing
  li t0, 0x2  # SSTATUS_SIE bit
  csrs sstatus, t0

  # Call C trap handler (NEVER RETURNS - it calls trap_return or panics)
  # Pass pointer to trap frame in task struct
  addi a0, tp, 120  # a0 = &current_task->tf
  call trap_handler

  # Should never reach here
  j .

trap_from_kernel:
  # Coming from kernel mode:
  # sp = 0 (from sscratch), sscratch = kernel stack pointer
  # Swap back to restore kernel stack
  csrrw sp, sscratch, sp
  # Now: sp = kernel stack (restored), sscratch = 0

  # Save t0 to sscratch FIRST before using it as scratch
  csrw sscratch, t0

  # Save trap frame on kernel stack (NOT in task struct)
  # We need 288 bytes for the trap frame
  addi sp, sp, -288

  # Save all registers to stack
  sd ra, 0(sp)
  # Save kernel sp (need to add 288 back since we just decremented it)
  addi t0, sp, 288
  sd t0, 8(sp)
  sd gp, 16(sp)
  # Don't save tp - it always points to current_task
  sd t1, 40(sp)
  sd t2, 48(sp)
  sd s0, 56(sp)
  sd s1, 64(sp)
  sd a0, 72(sp)
  sd a1, 80(sp)
  sd a2, 88(sp)
  sd a3, 96(sp)
  sd a4, 104(sp)
  sd a5, 112(sp)
  sd a6, 120(sp)
  sd a7, 128(sp)
  sd s2, 136(sp)
  sd s3, 144(sp)
  sd s4, 152(sp)
  sd s5, 160(sp)
  sd s6, 168(sp)
  sd s7, 176(sp)
  sd s8, 184(sp)
  sd s9, 192(sp)
  sd s10, 200(sp)
  sd s11, 208(sp)
  sd t3, 216(sp)
  sd t4, 224(sp)
  sd t5, 232(sp)
  sd t6, 240(sp)

  # Save CSRs
  csrr t0, sepc
  sd t0, 248(sp)
  csrr t0, sstatus
  sd t0, 256(sp)
  csrr t0, stval
  sd t0, 264(sp)
  csrr t0, scause
  sd t0, 272(sp)

  # Now save the actual t0 value (we used it for scratch above)
  csrr t0, sscratch  # Get old t0 if we saved it, or 0
  sd t0, 32(sp)

  # Call trap handler with pointer to trap frame on stack
  # DO NOT touch task->tf - that contains the user context!
  mv a0, sp  # a0 = trap frame pointer (on kernel stack)
  call trap_handler

  # If trap_handler returns, restore from stack and return to kernel
  # Restore CSRs
  ld t0, 248(sp)
  csrw sepc, t0
  ld t0, 256(sp)
  csrw sstatus, t0

  # CRITICAL: Restore sscratch to 0 for kernel mode
  # When returning to kernel mode, sscratch must be 0 so the next trap
  # can detect we're coming from kernel (trap_vector line 14: beqz sp, trap_from_kernel)
  csrw sscratch, zero

  # Restore all general purpose registers
  ld ra, 0(sp)
  ld gp, 16(sp)
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a0, 72(sp)
  ld a1, 80(sp)
  ld a2, 88(sp)
  ld a3, 96(sp)
  ld a4, 104(sp)
  ld a5, 112(sp)
  ld a6, 120(sp)
  ld a7, 128(sp)
  ld s2, 136(sp)
  ld s3, 144(sp)
  ld s4, 152(sp)
  ld s5, 160(sp)
  ld s6, 168(sp)
  ld s7, 176(sp)
  ld s8, 184(sp)
  ld s9, 192(sp)
  ld s10, 200(sp)
  ld s11, 208(sp)
  ld t3, 216(sp)
  ld t4, 224(sp)
  ld t5, 232(sp)
  ld t6, 240(sp)

  # Restore kernel sp and return to kernel mode
  ld sp, 8(sp)

  sret

# trap_return(trap_frame_t *tf)
# Restores CPU state from trap frame and returns to interrupted execution
# NOTE: Caller must set sscratch to kernel stack top before calling this
.global trap_return
trap_return:
  # a0 = trap_frame pointer
  # Save trap_frame pointer - a0 might be clobbered by function call
  mv s0, a0

  # Before returning to user space, check for pending signals
  # The trap_frame pointer is already in a0, which is the first argument
  call check_and_deliver_signals

  # Restore trap_frame pointer from s0 (callee-saved register)
  mv sp, s0

  # Restore sscratch to kernel stack TOP, before restoring general regs
  # This is critical! When the next trap occurs, trap_vector will do:
  #   csrrw sp, sscratch, sp
  # which swaps sp (user stack) with sscratch (must be kernel stack TOP)
  #
  # The kernel stack TOP is stored in current_task->kernel_context.stack_start + KERNEL_STACK_SIZE
  # kernel_context.stack_start is at offset 112
  # We use t0 and t1 as scratch, but restore them later from the trap frame
  ld t0, 112(tp)       # t0 = kernel_context.stack_start
  li t1, 16384         # t1 = KERNEL_STACK_SIZE (16KB = 16384 bytes)
  add t0, t0, t1       # t0 = stack_start + KERNEL_STACK_SIZE
  csrw sscratch, t0    # sscratch = kernel stack TOP

  # Restore CSRs (sepc and sstatus)
  ld t0, 248(sp)
  csrw sepc, t0
  ld t0, 256(sp)
  csrw sstatus, t0

  # Restore all general purpose registers
  ld ra, 0(sp)
  # Don't restore sp yet, we need it to access the frame
  ld gp, 16(sp)
  # Don't restore tp - it must always point to current_task
  ld t0, 32(sp)
  ld t1, 40(sp)
  ld t2, 48(sp)
  ld s0, 56(sp)
  ld s1, 64(sp)
  ld a0, 72(sp)
  ld a1, 80(sp)
  ld a2, 88(sp)
  ld a3, 96(sp)
  ld a4, 104(sp)
  ld a5, 112(sp)
  ld a6, 120(sp)
  ld a7, 128(sp)
  ld s2, 136(sp)
  ld s3, 144(sp)
  ld s4, 152(sp)
  ld s5, 160(sp)
  ld s6, 168(sp)
  ld s7, 176(sp)
  ld s8, 184(sp)
  ld s9, 192(sp)
  ld s10, 200(sp)
  ld s11, 208(sp)
  ld t3, 216(sp)
  ld t4, 224(sp)
  ld t5, 232(sp)
  ld t6, 240(sp)

  # Finally restore user sp and return to user mode
  # (sscratch was already set earlier, before we restored general regs)
  ld sp, 8(sp)
  sret
