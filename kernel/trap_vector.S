.section .text
.global trap_vector
.align 4

trap_vector:
  # tp register points to current_task (task_t*)
  # task_t.kernel_context is at offset 0 (120 bytes)
  # task_t.tf is at offset 120 (288 bytes)

  # Swap sp with sscratch to get kernel stack
  csrrw sp, sscratch, sp
  # Now: sp = kernel stack top, sscratch = user stack pointer

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

  # DEBUG: Print that we entered trap_vector
  la a0, trap_vector_entry_msg
  call printk

  # Call C trap handler (NEVER RETURNS - it calls trap_return or panics)
  # No arguments needed - trap_handler accesses current_task->tf directly
  call trap_handler

  # Should never reach here
  j .

# trap_return(trap_frame_t *tf)
# Restores CPU state from trap frame and returns to interrupted execution
# NOTE: Caller must set sscratch to kernel stack top before calling this
.global trap_return
trap_return:
  # a0 = trap_frame pointer
  mv sp, a0

  # Restore sscratch to kernel stack FIRST, before restoring general regs
  # This is critical! When the next trap occurs, trap_vector will do:
  #   csrrw sp, sscratch, sp
  # which swaps sp (user stack) with sscratch (must be kernel stack)
  #
  # The kernel stack top is stored in current_task->kernel_context.sp
  # We can get it via: tp + 0 (since kernel_context is at offset 0)
  # We use t0 as scratch, but restore it later from the trap frame
  ld t0, 0(tp)
  csrw sscratch, t0

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

.section .rodata
trap_vector_entry_msg:
  .string "[TRAP_VECTOR] Entered\n"
