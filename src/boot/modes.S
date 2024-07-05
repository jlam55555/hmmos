	## Utilities for moving between (un)real mode and protected
	## mode. I don't plan to support v8086 mode, it's easier to
	## just switch back to real mode in the few cases I need to.
	##
	## In this file, explicitly label labels with .code16 or
	## .code32 since we'll be switching back and forth.
	.section .text

	.globl enter_prot_mode
	.globl enter_unreal_mode
	.globl pmode_to_urmode

	## Enter 32-bit protected mode from (un)real mode.
	##
	## Note: Do not enable interrupts until the IDT is set up.
	##
	## - Input: %eax (pmode entry function)
	## - Clobbers: %eax
	.code16
enter_prot_mode:
	cli
	push %eax

	lgdt .Lgdt_desc
	mov %cr0, %eax
	or $1, %al		# Set PE bit.
	mov %eax, %cr0
	jmp $0x08, $.Lenter_prot_mode__setup

	## TODO: we should probably make sure stack is 4-byte
	## aligned. Practically this doesn't happen since %sp should
	## be its original value once we enter prot mode, but it's
	## probably a good thing to enforce.

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
	jmp *%eax

	## Enter unreal mode from real mode. This actually enters
	## protected mode, updates the code and data segment
	## descriptor caches, then returns to real mode.
	## - Clobbers: %eax
	.code16
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

	## Enter unreal mode from protected mode.
	##
	## This should only be used for running a small (un)real-mode
	## program from protected mode, e.g., copying bytes or
	## updating the video settings.
	##
	## Makes the following assumptions:
	## - We start off in 32-bit pmode (from enter_pmode).
	## - Paging is disabled.
	## - Interrupts are disabled and the real mode IDT is still
	##   loaded.
	## - Stack is still valid in real mode.
	##
	## - Input: %ax (real-mode entry point)
	## - Clobbers: %eax, %cr0, segment regs
	.code32
pmode_to_urmode:
	## Technically stack is not 4-byte aligned now, but this is
	## okay temporarily.
	push %ax

	## Don't need cli since interrupts should already be disabled.
	## Don't need to disable paging either.

	## Keep 32-bit protected mode data segment selectors for
	## unreal mode.

	## Far jump to 16-bit protected mode.
	jmp $0x28, $.Lpmode_to_urmode__16bit_pmode

	.code16
.Lpmode_to_urmode__16bit_pmode:
	## Disable protected mode.
	mov %cr0, %eax
	and $0xFE, %al
	mov %eax, %cr0

	## Far jump to set %cs.
	jmp $0x0, $.Lpmode_to_urmode__rmode
.Lpmode_to_urmode__rmode:
	## Set segment registers.
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss

	## Re-enable real-mode interrupts.
	sti

	pop %ax
	jmp *%ax

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
