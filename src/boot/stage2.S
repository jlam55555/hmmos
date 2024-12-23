	## There's not much significance in the term "stage 2" other
	## than the fact that this comes after boot.S and
	## stage1_5.S. I initially thought it sounded like GRUB's
	## stage 2 but this is much less complicated.
	##
	## At this point, we've entered 32-bit protected mode, and
	## only should switch back to real mode for select BIOS
	## functions (e.g., copying the kernel from disk or setting
	## video settings).
	##
	## Since we're in 32-bit mode, we have the x86 cdecl calling
	## convention and can interop with C code.
	##
	## The main goals of this stage are to:
	## - Clean up the E820 memory map loaded in stage 1.5.
	## - Load the kernel and service its bootloader requests.
	## - Set up paging.
	## - Jump to the kernel.
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

	## Log some info about the partitions. Shouldn't have any side
	## effects.
	call read_mbr_partitions

	## Read the kernel into physical memory, and return its
	## starting physical address.
	call copy_kernel
	test %eax, %eax
	jz .Lpmode_entry__err
	## Push retval (kernel physical address), which will be the
	## input parameter to pt_setup.
	push %eax

	call check_cpuid_features
	test %eax, %eax
	jz .Lpmode_entry__err

	## Check paging before (this is expected to fail) ...
	call check_paging_setup
	test %eax, %eax
	jnz .Lpmode_entry__err

	call pt_setup
	test %eax, %eax
	jz .Lpmode_entry__err
	## Cleanup input parameter.
	pop %eax

	## ... and after (this is expected to succeed).
	call check_paging_setup
	test %eax, %eax
	jz .Lpmode_entry__err

	call e820_mm_print

	## Clear registers and jump to kernel! Don't clear %esp but
	## we'll set it to a HHDM address in jump_to_kernel.
	xor %eax, %eax
	xor %ebx, %ebx
	xor %ecx, %ecx
	xor %edx, %edx
	xor %ebp, %ebp
	xor %edi, %edi
	xor %esi, %esi
	jmp jump_to_kernel

.Lpmode_entry__err:
	push $.Lpmode_entry__err__str
	call console_puts
.Lpmode_entry__err__loop:
	hlt
	jmp .Lpmode_entry__err__loop

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

	.section .rodata
.Lpmode_entry__err__str:
	.asciz "encountered error in stage2"
