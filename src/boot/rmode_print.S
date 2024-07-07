	## Simple print functions for real-mode printing.  These
	## aren't accessible outside real mode since they depend on
	## BIOS interrupts.
	##
	## The console_*() functions are better for printing but
	## should only be used in protected mode.
	.code16
	.globl rmode_print
	.globl rmode_println
	.globl rmode_printb
	.globl rmode_printw
	.globl rmode_printl

	.globl newline
	.globl empty_str

	## Very simple string printing function. Needs to be in
	## .text.boot since it's used in the MBR (before stage 1.5 is
	## loaded).
	##
	## - Input: %di (char*)
	## - Clobbers: %ax
	.section .text.boot
rmode_print:
	mov $0x0E, %ah
.Lrmode_print__loop:
	mov (%di), %al
	test %al, %al
	jz .Lrmode_print__exit
	int $0x10
	inc %di
	jmp .Lrmode_print__loop
.Lrmode_print__exit:
	ret

	## - Input: %di
	## - Clobbers: %ax
	.section .text
rmode_println:
	call rmode_print
	mov $newline, %di
	call rmode_print
	ret

	## Helper to format a single nybble.
	## - Preconditions: %al < 16
	## - Input: %al (nybble), %si (output buffer)
	## - Side effects: increments %si.
.Lrmode_print_nybble:
	cmp $10, %al
	jge .Lrmode_print_nybble__alpha
	add $'0, %al
	jmp .Lrmode_print_nybble__exit
.Lrmode_print_nybble__alpha:
	add $'7, %al
.Lrmode_print_nybble__exit:
	movb %al, (%si)
	inc %si
	ret

	## Helper to format a single byte without prefix or null
	## terminator.
	## - Input: %al (n), %si (output memory)
	## - Clobbers: %al
	## - Side effects: increments %si by 2.
.Lrmode_print_byte:
	push %bx
	## Upper nybble.
	mov %al, %bl
	shr $4, %al
	call .Lrmode_print_nybble
	## Lower nybble.
	mov %bl, %al
	and $0x0F, %al
	call .Lrmode_print_nybble
	pop %bx
	ret

	## Print a single byte as hexidecimal, prefixed with "0x".
	## - Input: %al (n)
	## - Clobbers: %al
rmode_printb:
	push %si
	push %di
	mov $.Lrmode_print_byte_buf+2, %si
	call .Lrmode_print_byte
	movb $0x0, (%si)	# Null-terminate.
	mov $.Lrmode_print_byte_buf, %di
	call rmode_print
	pop %di
	pop %si
	ret

	## Print a word as hexidecimal, prefixed with "0x".
	## - Input: %eax (n)
	## - Clobbers: %eax
rmode_printw:
	push %si
	push %di
	push %cx
	mov $.Lrmode_print_byte_buf+2, %si

	bswap %eax
	shr $16, %eax
	call .Lrmode_print_byte
	shr $8, %ax
	call .Lrmode_print_byte

	movb $0x0, (%si)	# Null-terminate.
	mov $.Lrmode_print_byte_buf, %di
	call rmode_print

	pop %cx
	pop %di
	pop %si
	ret

	## Print a doubleword as hexidecimal, prefixed with "0x".
	## - Input: %eax (n)
	## - Clobbers: %eax
rmode_printl:
	push %si
	push %di
	push %cx
	mov $.Lrmode_print_byte_buf+2, %si

	bswap %eax
	call .Lrmode_print_byte
	shr $8, %eax
	call .Lrmode_print_byte
	shr $8, %eax
	call .Lrmode_print_byte
	shr $8, %ax
	call .Lrmode_print_byte

	movb $0x0, (%si)	# Null-terminate.
	mov $.Lrmode_print_byte_buf, %di
	call rmode_print

	pop %cx
	pop %di
	pop %si
	ret

	.section .data

	## Global buffer used for rmode_print_byte* functions. Clearly
	## not thread-safe but we're not too worried about that here
	## (except possibly interrupts).
	##
	## Up to 8 bytes (doubleword * 2char/byte) + 2 byte (0x)
	## prefix + 1 byte NULL.
.Lrmode_print_byte_buf:
	.ascii "0x"
	.quad 0x0
	.byte 0x0

	.section .rodata
newline:
	.asciz "\r\n"
empty_str:
	.asciz ""

