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
	.globl copy_bytes

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

	call augment_bootloader_text_stack_sections
	test %eax, %eax
	jz .Lpmode_entry__err

	call read_mbr_partitions

	call copy_kernel
	test %eax, %eax
	jz .Lpmode_entry__err
	## Push retval (kernel physical address), which will be the
	## input parameter to pt_setup.
	push %eax

	call check_cpuid_features
	test %eax, %eax
	jz .Lpmode_entry__err

	## Check paging before ...
	call check_paging_setup

	call pt_setup
	test %eax, %eax
	jz .Lpmode_entry__err
	## Cleanup input parameter.
	pop %eax

	## ... and after.
	call check_paging_setup

	call e820_mm_print

	## TODO: jump to the kernel once we have a compiled kernel.
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

	## `extern void copy_bytes(void *mem_addr,
	##                         void *disk_addr,
	##                         uint32_t len);`
	##
	## Copy bytes from disk to RAM. Uses a simple unreal-mode
	## program to access BIOS INT 0x13 functions.
	##
	## I think the MBR can also use this to bootstrap stage 1.5,
	## as long as the disk address packet is in the MBR. However,
	## the stage 1.5 case is very simple so I'm going to leave it
	## alone to avoid complicating the MBR.
	##
	## - Precondition: disk_addr and len are sector-aligned
	##   (512-byte-aligned)
copy_bytes:
	enter $0, $0
	push %edi
	push %esi
	push %ebx

	## See below loop for description of %ebx, %ecx, %edi.
	mov 0x08(%ebp), %ecx	# mem_addr
	mov 0x0C(%ebp), %ebx	# disk_addr; convert to sectors
	shr $9, %ebx
	mov 0x10(%ebp), %edi	# length; convert to end sector
	shr $9, %edi
	add %ebx, %edi

	mov $.Lcopy_bytes__rmode, %ax
	jmp pmode_to_urmode

	.code16
.Lcopy_bytes__rmode:
	## Set up disk address packet.
	movw $0x1, .Lcopy_bytes__disk_address_packet+0x2
	movl $.Lcopy_bytes__buf, .Lcopy_bytes__disk_address_packet+0x4
	movl $0x0, .Lcopy_bytes__disk_address_packet+0x6
	mov $.Lcopy_bytes__disk_address_packet, %si
	## For now, just zero the top bytes of the LBA address, since
	## we only support 32-bit disk addresses. This can be changed
	## in the future though.
	movl $0x0, .Lcopy_bytes__disk_address_packet+0xC

	## We need to loop since the BIOS function is still limited to
	## 20-bit memory addresses, but we may be copying more than
	## that/at higher memory addresses. So we copy 1 sector at a
	## time and copy it to the proper location.
	##
	## - %ebx stores the current sector LBA
	## - %ecx stores the current destination address
	## - %edi stores the end sector LBA
	##
	## Logically this looks like:
	## ```
	## unsigned *dest = <destination in RAM>;
	## while (sector < end) {
	##     bios_int0x13_copy_sector(sector, tmp_buf);
	##     // simple custom memcpy
	##     unsigned *src = tmp_buf;
	##     do {
	##         *(dst++) = *(src++);
	##     } while (src & 0x1FF);
	##     ++sector;
	## }
	## ```
.Lcopy_bytes__rmode__loop:
	mov %ebx, .Lcopy_bytes__disk_address_packet+0x8
	mov $0x42, %ah
	mov $0x80, %dl
	int $0x13

	## Copy sector from temporary buffer to memory destination.
	## We use %eax and %edx as temporary registers here.
	mov $.Lcopy_bytes__buf, %eax
.Lcopy_bytes__rmode__loop__copy_sector:
	mov (%eax), %edx
	mov %edx, (%ecx)
	add $4, %eax
	add $4, %ecx
	test $0x1FF, %eax
	jz .Lcopy_bytes__rmode__loop__copy_sector__exit
	jmp .Lcopy_bytes__rmode__loop__copy_sector

.Lcopy_bytes__rmode__loop__copy_sector__exit:
	inc %ebx
	cmp %edi, %ebx
	jge .Lcopy_bytes__rmode__exit
	jmp .Lcopy_bytes__rmode__loop

.Lcopy_bytes__rmode__exit:
	mov $.Lcopy_bytes__exit, %ax
	jmp enter_prot_mode

	.code32
.Lcopy_bytes__exit:
	pop %ebx
	pop %esi
	pop %edi
	leave
	ret

	.section .data
.Lcopy_bytes__disk_address_packet:
	.byte 0x10		# packet size
	.byte 0x00		# reserved
	.word 0x00		# number of sectors to transfer
	.long 0x00		# target segment:offset
	.quad 0x00		# LBA

	.align 512
.Lcopy_bytes__buf:
	.fill 512
