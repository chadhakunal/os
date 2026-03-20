OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS {
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS {
	. = 0x80200000;
	_kernel_start = .;
	
	.text.init : { *(.text.init) } :text
	.text   : { *(.text) *(.text.*) } :text
	.rodata : { *(.rodata) *(.rodata.*) } :text
	
	.data   : { *(.data) *(.data.*) *(.sdata) *(.sdata.*) } :data
	.tarfs  : { *(.tarfs) } :data
	
	_bss_start = .;
	.bss (NOLOAD) : { *(.bss) *(.bss.*) *(.sbss) *(.sbss.*) *(COMMON) } :data
	_bss_end = .;
	
	. = ALIGN(16);
	. += 16384;
	_stack_top = .;
	_end = .;
	_kernel_end = .;
}
