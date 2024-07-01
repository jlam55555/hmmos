	## Real-mode util functions. Some of this logic can probably
	## be shared with protected-mode (32-bit) functions, but I'll
	## keep them separate for now.
	.code16
	.text

	.globl dump_regs
	.globl get_ip

	## Gets IP for the calling instruction.
	##
	## `(%sp)` is not a valid 16-bit addressing mode (`(%esp)` is,
	## but we can't guarantee that the upper bytes of $esp are
	## zeroed), so we need a small workaround.
	##
	## - Output: %ax (ip)
get_ip:
	push %bp
	mov %sp, %bp
	mov 2(%bp), %ax
	pop %bp
	ret

	## Helper function for dump_regs. Shouldn't have side effects.
	## - Input: %eax (reg val), %di (reg name string)
	## - Clobbers: %eax
.Ldump_reg:
	push %ax
	call print
	pop %ax
	call printl
	call println
	ret

	## Print GPRs. We don't need to print segment registers in
	## most cases since we always set them to zero.
	## Shouldn't have any side effects.
dump_regs:
	push %esp
	push %eax
	push %ebp
	push %edi
	push %ebx
	mov %sp, %bp

	mov $.Lax, %di
	call .Ldump_reg
	mov $.Lbx, %di
	mov 0x0(%bp), %eax
	call .Ldump_reg
	mov $.Lcx, %di
	mov %ecx, %eax
	call .Ldump_reg
	mov $.Ldx, %di
	mov %edx, %eax
	call .Ldump_reg
	mov $.Lsi, %di
	mov %esi, %eax
	call .Ldump_reg
	mov $.Ldi, %di
	mov 0x04(%bp), %eax
	call .Ldump_reg
	## %sp is a special case. To get the value that the caller
	## %sees, we must push it first/pop it last, and add 2 since
	## %the push instruction decrements %sp first.
	mov $.Lsp, %di
	mov 0x10(%bp), %eax
	add $2, %eax
	call .Ldump_reg
	mov $.Lbp, %di
	mov 0x08(%bp), %eax
	call .Ldump_reg

	## Similar to get_eip
	mov $.Lip, %di
	mov 0x14(%bp), %ax
	## Clear upper bits of %eax since we only have a 16-bit value.
	and %eax, 0xFF
	call .Ldump_reg

	pop %ebx
	pop %edi
	pop %ebp
	pop %eax
	pop %esp
	ret

.Lax:	.asciz "%eax="
.Lbx:	.asciz "%ebx="
.Lcx:	.asciz "%ecx="
.Ldx:	.asciz "%edx="
.Lsi:	.asciz "%esi="
.Ldi:	.asciz "%edi="
.Lsp:	.asciz "%esp="
.Lbp:	.asciz "%ebp="
.Lip:	.asciz "%eip="
