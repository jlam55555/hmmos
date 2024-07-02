	## This runs in the same environment as boot.S (real mode).
	## The only difference is that boot.S is loaded by the BIOS,
	## whereas this "stage 1.5" bootloader is loaded by the stage
	## 1 code (boot.S).
	##
	## We have a lot more freedom here simply in terms of
	## space. boot.S is constrained to a single sector (512
	## bytes), whereas we get 62 sectors here (same as GRUB).
	##
	## This is not a complicated bootloader so we won't use all
	## that space, but it's there if needed.
	.code16
	.section .text

	.globl stage_1_5

stage_1_5:
	## Dumping registers and testing the print functions.
	## Not wise to modify %esp so I'm leaving it.
	mov $.Ldump_regs_msg, %di
	call println

	mov $0x1234, %ax
	mov $0x5678, %bx
	mov $0x9ABC, %cx
	mov $0xDEF0, %dx
	mov $0x1234, %si
	mov $0x5678, %di
	mov $0xDEADBEEF, %ebp
	call dump_regs

	mov $.Lenable_a20_msg, %di
	call println
	call enable_a20

	## Enter protected mode (and then perhaps unreal mode to load
	## the kernel).
	mov $.Lenable_prot_mode_msg, %di
	call println
	## TODO

	call done

	.section .rodata
.Ldump_regs_msg:
	.asciz "Current register values:"
.Lenable_a20_msg:
	.asciz "Enabling A20 line..."
.Lenable_prot_mode_msg:
	.asciz "Entering protected mode..."
