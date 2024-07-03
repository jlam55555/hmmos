	## Utilities for moving between (un)real mode and protected
	## mode. I don't plan to support v8086 mode, it's easier to
	## just switch back to real mode in the few cases I need to.
	.code16
	.section .text

	.globl enter_prot_mode
	.globl enter_unreal_mode

	## Enter 32-bit protected mode from (un)real mode.
	##
	## Note: Do not enable interrupts until the IDT is set up.
	##
	## - Input: %eax (pmode entry function)
	## - Clobbers: %eax
enter_prot_mode:
	cli
	push %eax

	lgdt .Lgdt_desc
	mov %cr0, %eax
	or $1, %al		# Set PE bit.
	mov %eax, %cr0
	jmp $0x08, $.Lenter_prot_mode__setup

	## .code32 isn't too necessary because these instructions are
	## valid in 16-bit mode and 32-bit mode, but might as well.
	.code32
.Lenter_prot_mode__setup:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	pop %eax
	jmp %eax

	.code16

	## Enter unreal mode from real mode. This actually enters
	## protected mode, updates the code and data segment
	## descriptor caches, then returns to real mode.
	## - Clobbers: %eax
enter_unreal_mode:
	cli
	lgdt .Lgdt_desc
	mov %cr0, %eax
	or $0x01, %al
	mov %eax, %cr0
	## Use unreal code segment (segment 5).
	jmp $0x28, $.Lenter_unreal_mode__pmode
.Lenter_unreal_mode__pmode:
	## Use 32-bit data segment (segment 2).
	mov $0x10, %ax
	mov %ax, %ds
	mov %cr0, %eax
	and $0xFE, %al
	mov %eax, %cr0
	jmp $0x0, $.Lenter_unreal_mode__urmode
.Lenter_unreal_mode__urmode:
	xor %ax, %ax
	mov %ax, %ds
	sti
	ret

	.section .rodata
.Lgdt_desc:
	.word (.Lgdt__end - .Lgdt - 1)
	.word .Lgdt, 0x0000

.Lgdt:
	.quad 0x0
.Lgdt__ring0_code:
	.word 0xFFFF, 0x0000
	.byte 0x00, 0b10011010, 0b11001111, 0x00
.Lgdt__ring0_data:
	.word 0xFFFF, 0x0000
	.byte 0x00, 0b10010010, 0b11001111, 0x00
.Lgdt__ring3_code:
	.word 0xFFFF, 0x0000
	.byte 0x00, 0b11111010, 0b11001111, 0x00
.Lgdt__ring3_data:
	.word 0xFFFF, 0x0000
	.byte 0x00, 0b11110010, 0b11001111, 0x00
.Lgdt__unreal:
	## Same as ring0_code but with limit 0xFFFF and 16-bit mode.
	.word 0xFFFF, 0x0000
	.byte 0x00, 0b10011010, 0b00000000, 0x00
.Lgdt__end:
