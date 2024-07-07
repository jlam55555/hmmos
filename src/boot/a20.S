	## Enabling the A20 line. This follows the (fairly extensive
	## for what seems like a simple task) guide on OSDev.
	.code16
	.section .text

	.globl enable_a20

	## Checks if A20 line is enabled. See explanation in boot.md.
	## Shouldn't have any side effects.
	##
	## - Output: %ax (1 iff A20 line already set, else 0)
.Lcheck_a20:
	pushf
	push %ds
	push %es
	push %di
	push %si
	cli

	## es:di = 0x0000:0x0500 = 0x000500
	## ds:si = 0xFFFF:0x0510 = 0x100500
	xor %ax, %ax
	mov %ax, %es
	not %ax
	mov %ax, %ds
	mov $0x0500, %di
	mov $0x0510, %si

	## Save old values of (es:di) and (ds:si).
	mov %es:(%di), %al
	push %ax
	mov %ds:(%si), %al
	push %ax

	## The real experiment: Set (es:di) and (ds:si) to different
	## values, and see if setting (ds:si) also sets (es:di).
	movb $0x12, %es:(%di)
	movb $0xFF, %ds:(%si)
	cmpb $0xFF, %es:(%di)

	## Cleanup.
	pop %ax
	mov %al, %ds:(%si)
	pop %ax
	mov %al, %es:(%di)

	mov $0, %ax
	je .Lcheck_a20__exit
	mov $1, %ax

.Lcheck_a20__exit:
	pop %si
	pop %di
	pop %es
	pop %ds
	popf
	ret

	## Enable the A20 line if not already enabled.
	## Clobbers: %ax, %di
enable_a20:
	call .Lcheck_a20
	test %ax, %ax
	jz .Lenable_a20__do_enable
	## Already enabled, nothing to do here.
	mov $.La20_already_enabled_msg, %di
	call rmode_println
	ret

.Lenable_a20__do_enable:
	## TODO: The A20 line is enabled on QEMU so I don't have an
	## easy way to test this... for now simply die if we encounter
	## this. We can deal with this when we get to it.
	mov $.La20_disabled_msg, %di
	call rmode_println
	jmp rmode_done

	.section .rodata
.La20_already_enabled_msg:
	.asciz "A20 already enabled, skipping..."
.La20_disabled_msg:
