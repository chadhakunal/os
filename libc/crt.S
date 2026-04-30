.section .text
.global _start
_start:
  # Clear frame pointer
  mv fp, zero

  # Call main(argc, argv, envp)
  # For now, pass 0 for all arguments
  li a0, 0        # argc = 0
  li a1, 0        # argv = NULL
  li a2, 0        # envp = NULL
  call main

  # main returned - call exit with return value
  # Return value is in a0
  call exit

  # Should never reach here, but just in case
  j .
