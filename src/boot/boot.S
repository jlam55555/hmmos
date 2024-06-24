	## See /notes/boot.md
	.code16
	.text
_start:
	## Display char: http://www.ctyme.com/intr/rb-0106.htm
	mov $0x0E, %ah

	## Print characters 0-255.
	xor %al, %al
loop1:
	int $0x10
	inc %al
	jnz loop1

	## Hello world.
	mov $msg, %bp

loop2:
	mov (%bp), %al
	test %al, %al
	jz .
	int $0x10
	inc %bp
	jmp loop2

msg:	.asciz "Hello, world!"

	.skip 510 - (. - _start), 0
	.byte 0x55, 0xAA
