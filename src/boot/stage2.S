	## At this point, we're in protected mode and shouldn't try to
	## use any of the real-mode utilities.
	##
	## It is possible to switch back to real mode, but it's easier
	## to do everything that need be done in real mode first
	## before switching to pmode so we don't need to carefully
	## save state.
	.code32
	.section .text

	.globl prot_mode_entry

prot_mode_entry:
	movw $0x0F01, 0xB8000
.Lloop:
	hlt
	jmp .Lloop
