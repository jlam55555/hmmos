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
	.globl get_cpuid_features
	.globl enable_paging

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

	call check_cpuid_features
	test %eax, %eax
	jz .Lpmode_entry__err

	## Check paging before ...
	call check_paging_setup

	call pt_setup
	test %eax, %eax
	jz .Lpmode_entry__err

	## ... and after.
	call check_paging_setup

	call e820_mm_print

	## TODO: parse and jump to the kernel.
	## For now we just spin here.
.Ljmp_kernel:
	hlt
	jmp .Ljmp_kernel

.Lpmode_entry__err:
	hlt
	jmp .Lpmode_entry__err

	## `extern void get_cpuid_features(struct cpuid_features*);`
	## Returns CPUID feature information as a 64-bit value.
get_cpuid_features:
	mov $1, %eax
	cpuid
	mov 4(%esp), %eax
	mov %ecx, (%eax)
	mov %edx, 4(%eax)
	ret

	## `extern void enable_paging(struct page_directory_entry*);`
	## Enables paging (assumes protected mode already set).
	## Also enables PSE.
enable_paging:
	## Enable PSE (hugepages).
	mov %cr4, %eax
	or $0x10, %eax
	mov %eax, %cr4

	## Set page directory table.
	mov 4(%esp), %eax
	mov %eax, %cr3

	## Enable paging.
	mov %cr0, %eax
	or $0x80000000, %eax
	mov %eax, %cr0
	ret
