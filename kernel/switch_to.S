.section .text
.global switch_to
.align 4

switch_to:
  # a0 = me (current task)
  # a1 = next (next task)
  # a2 = last (optional for now)
  # Save kernel context into the kernel_context object
  ld a0, 0(sp) # Store me onto current stack
  addi sp, sp, -8
  ld a2, 0(sp) # Store last onto stack, note last is a pointer to the location of the *last on the stack of me (at this point)
  addi sp, sp, -8
  
