	## At this point, we're in protected mode and shouldn't try to
	## use any of the real-mode utilities.
	##
	## It is possible to switch back to real mode, but it's easier
	## to do everything that need be done in real mode first
	## before switching to pmode so we don't need to carefully
	## save state.
	.code32
	.section .text

	.globl pmode_entry

	## Entering protected mode. Segment registers and %esp should
	## be correctly set upon entering this function.
pmode_entry:
	xor %eax, %eax
	xor %ebx, %ebx
	xor %ecx, %ecx
	xor %ebp, %ebp
	xor %edi, %edi
	xor %esi, %esi
	xor %edx, %edx

	call pt_setup
	test %eax, %eax
	jz .Lpmode_entry__err

	call e820_mm_print

	## TODO: parse and jump to the kernel.
	## For now we just spin here.
.Ljmp_kernel:
	hlt
	jmp .Ljmp_kernel

.Lpmode_entry__err:
	hlt
	jmp .Lpmode_entry__err
