	## Simple print function lives in boot.S. These are some
	## additional utilities for real-mode printing.
	.code16
	.text

	.globl println
	.globl printb
	.globl printw
	.globl printl

	.globl newline
	.globl empty_str

	## - Input: %di
	## - Clobbers: %ax
println:
	call print
	mov $newline, %di
	call print
	ret

	## Helper to format a single nybble.
	## - Preconditions: %al < 16
	## - Input: %al (nybble), %si (output buffer)
	## - Side effects: increments %si.
.Lprint_nybble:
	cmp $10, %al
	jge .Lprint_nybble__alpha
	add $'0, %al
	jmp .Lprint_nybble__exit
.Lprint_nybble__alpha:
	add $'7, %al
.Lprint_nybble__exit:
	movb %al, (%si)
	inc %si
	ret

	## Helper to format a single byte without prefix or null
	## terminator.
	## - Input: %al (n), %si (output memory)
	## - Clobbers: %al
	## - Side effects: increments %si by 2.
.Lprint_byte:
	push %bx
	## Upper nybble.
	mov %al, %bl
	shr $4, %al
	call .Lprint_nybble
	## Lower nybble.
	mov %bl, %al
	and $0x0F, %al
	call .Lprint_nybble
	pop %bx
	ret

	## Print a single byte as hexidecimal, prefixed with "0x".
	## - Input: %al (n)
	## - Clobbers: %al
printb:
	push %si
	push %di
	mov $.Lprint_byte_buf+2, %si
	call .Lprint_byte
	movb $0x0, (%si)	# Null-terminate.
	mov $.Lprint_byte_buf, %di
	call print
	pop %di
	pop %si
	ret

	## Print a word as hexidecimal, prefixed with "0x".
	## - Input: %eax (n)
	## - Clobbers: %eax
printw:
	push %si
	push %di
	push %cx
	mov $.Lprint_byte_buf+2, %si

	bswap %eax
	shr $16, %eax
	call .Lprint_byte
	shr $8, %ax
	call .Lprint_byte

	movb $0x0, (%si)	# Null-terminate.
	mov $.Lprint_byte_buf, %di
	call print

	pop %cx
	pop %di
	pop %si
	ret

	## Print a doubleword as hexidecimal, prefixed with "0x".
	## - Input: %eax (n)
	## - Clobbers: %eax
printl:
	push %si
	push %di
	push %cx
	mov $.Lprint_byte_buf+2, %si

	bswap %eax
	call .Lprint_byte
	shr $8, %eax
	call .Lprint_byte
	shr $8, %eax
	call .Lprint_byte
	shr $8, %ax
	call .Lprint_byte

	movb $0x0, (%si)	# Null-terminate.
	mov $.Lprint_byte_buf, %di
	call print

	pop %cx
	pop %di
	pop %si
	ret

	.section .data

	## Global buffer used for print_byte* functions. Clearly not
	## thread-safe but we're not too worried about that here
	## (except possibly interrupts).
	##
	## Up to 8 bytes (doubleword * 2char/byte) + 2 byte (0x)
	## prefix + 1 byte NULL.
.Lprint_byte_buf:
	.ascii "0x"
	.quad 0x0
	.byte 0x0

	.section .rodata
newline:
	.asciz "\r\n"
empty_str:
	.asciz ""

