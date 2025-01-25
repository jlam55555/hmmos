	.section .text

	.globl enter_userspace

enter_userspace:
	mov $0x23, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	push $0x23 		# ss3
	push $0x10000000	# esp3
	pushf			# eflags3
	push $0x1B		# cs3
	push $0x10001000	# eip3
	iret

.Lfoobar:
	cli
