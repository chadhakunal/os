.section .text
.global task_entry
.align 4

# task_entry - Entry point for newly created tasks
#
# This is set as the 'ra' in kernel_context for new tasks.
# When switch_to() returns for the first time, it jumps here.
#
# Unlike normal returns from schedule(), this doesn't try to return
# anywhere - it directly enters user mode via trap_return().
#
# Note: tp register points to current_task
#       trap frame is at offset 120 from tp
task_entry:
    # Save ra temporarily
    mv s0, ra

    # Print debug message
    la a0, task_entry_msg
    call printk

    # Restore ra
    mv ra, s0

    # Calculate &current_task->tf
    # tp = current_task, tf is at offset 120
    addi a0, tp, 120

    # Call trap_return(&current_task->tf)
    # This never returns - does sret to user mode
    call trap_return

    # Should never reach here
    j .

.section .rodata
task_entry_msg:
    .string "[task_entry] Entered task_entry for new task\n"
