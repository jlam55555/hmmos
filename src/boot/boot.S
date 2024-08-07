	## Bootloader entry point. This needs to fit into the 512-byte
	## boot sector (MBR). See /notes/boot.md.
	##
	## This loads the rest of the bootloader into memory. Then we
	## jump into stage1_5.S, which sets up protected mode and
	## jumps into stage2.S. This does some protected-mode setup
	## and jumps into the kernel.
	.code16
	.section .text

	.globl rmode_done
	.globl mbr_start
mbr_start:
	## Clear registers (except %sp). We don't really depend on the
	## initial values of most of these GPRs (except %sp), so this
	## isn't too necessary. But might as well.
	xor %eax, %eax		# accumulator
	xor %ebx, %ebx		# base
	xor %ecx, %ecx		# counter
	xor %ebp, %ebp		# base pointer
	xor %edi, %edi		# destination
	xor %esi, %esi		# source
	xor %edx, %edx		# data

	## Clear segment registers.
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %fs
	mov %ax, %gs
	jmp $0x0, $.Lclear_cs	# Far jump to clear %cs.
.Lclear_cs:
	## Set stack just under the bootloader. We should have plenty
	## of space there, since the BDA ends at 0x0500 (and the
	## bootloader starts at 0x7C00).
	mov $mbr_start, %sp

	## Print hello message (might as well).
	mov $.Lwelcome_msg, %di
	call rmode_print

	## Load bootloader stage 1.5 into memory.
	mov $.Ldisk_address_packet, %si
	mov $0x42, %ah
	mov $0x80, %dl
	int $0x13

	jc .Lload_stage_1_5_err
	jmp stage_1_5

.Lload_stage_1_5_err:
	## If we run into this, it's debugger time. Since I'm too lazy
	## to put printing functions here.
	mov $.Lload_stage_1_5_err_msg, %di
	call rmode_print
	jmp rmode_done

	## System exit.
rmode_done:
	mov $.Lrmode_done_msg, %di
	call rmode_print
.Lrmode_done__loop:
	hlt
	jmp .Lrmode_done__loop

	.section .rodata
.Ldisk_address_packet:
	## Hardcoded to copy the 1.5 bootloader.
	.byte 0x10		# size
	.byte 0x0		# reserved
	.word 62		# number of sectors to transfer
	.long 0x00007E00	# target segment:offset
	.quad 0x1		# LBA
.Lwelcome_msg:
	.asciz "Welcome to HmmOS!\r\n"
.Lload_stage_1_5_err_msg:
	.asciz "Error loading stage 1.5 bootloader\r\n"
.Lrmode_done_msg:
	.asciz "done()\r\n"
