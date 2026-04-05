.section .text
.global trap_vector
.align 4

trap_vector:
    # Just call the C handler directly
    call trap_handler
    # Never returns (panics)
