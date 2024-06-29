	## See /notes/boot.md
	.code16
	.section .text.boot

	## NOCOMMIT
	.global print
	.global msg
	.global done
start:
	## Clear registers.
	mov $0, %ax		# accumulator
	mov %ax, %bx		# base
	mov %ax, %cx		# counter
	mov %ax, %bp		# base pointer
	mov %ax, %di		# destination
	mov %ax, %si		# source
	mov %ax, %dx		# data

	## (For now, assume segments regs are zeroed.)

	## Set stack just under the bootloader. We should have plenty
	## of space there, since the BDA ends at 0x0500 (and the
	## bootloader starts at 0x7C00).
	mov $start, %sp

	jmp c_entry

print:	## Text to print is in %di. %ax gets clobbered.
	## Similar to x86_64 fncall ABI.
	mov $0x0E, %ah
.loop:	mov (%di), %al
	test %al, %al
	jz .done
	int $0x10
	inc %di
	jmp .loop
.done:	ret

done:	hlt
	jmp done

	.section .rodata
msg:	.asciz "Hello, world!\r\n"
msg2:	.asciz "Second line\r\n"
