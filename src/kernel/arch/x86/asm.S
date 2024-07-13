	.section .text

	.globl outb
	.globl outw
	.globl outl
	.globl inb
	.globl inw
	.globl inl

outb:
	mov 4(%esp), %dx
	mov 8(%esp), %al
	out %al, %dx
	ret

outw:
	mov 4(%esp), %dx
	mov 8(%esp), %ax
	out %ax, %dx
	ret

outl:
	mov 4(%esp), %dx
	mov 8(%esp), %eax
	out %eax, %dx
	ret

inb:
	mov 4(%esp), %dx
	in %dx, %al
	ret

inw:
	mov 4(%esp), %dx
	in %dx, %ax
	ret

inl:
	mov 4(%esp), %dx
	in %dx, %eax
	ret
