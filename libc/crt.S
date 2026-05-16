.section .bss
.global environ
environ:
  .skip 8

.section .text
.global _start
_start:
  # Initialize global pointer
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop

  # Clear frame pointer
  mv fp, zero

  # Save envp (a2) into environ global
  la t0, environ
  sd a2, 0(t0)

  # Call main(argc, argv, envp)
  # Arguments are already in a0, a1, a2 from execve
  call main

  # main returned - call exit with return value
  # Return value is in a0
  call exit

  # Should never reach here, but just in case
  j .
