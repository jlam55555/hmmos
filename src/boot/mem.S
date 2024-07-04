	## Detect memory map using the BIOS 0xE820 function.
	.code16
	.text

	.globl detect_mem_map
	.globl e820_mem_map
	.globl e820_mm_max_entries

	.set E820_MM_MAX_ENTRIES, 32
	.set E820_ENTRY_SZ, 24
	.set E820_EAX_VAL, 0x0000E820
	.set E820_EDX_MAGIC, 0x534D4150

	## Writes the memory map from the INT 0x15, AX=0xE820 BIOS
	## function to e820_mem_map.
detect_mem_map:
	push %di
	push %eax
	push %ebx
	push %ecx
	push %edx

	## Initialization
	mov $e820_mem_map, %ax
	mov %ax, %di
	xor %ebx, %ebx
	mov $E820_EDX_MAGIC, %edx

.Ldetect_mem_map__loop:
	mov $E820_EAX_VAL, %eax
	mov $E820_ENTRY_SZ, %ecx
	int $0x15

	## CF is set after the last entry. In this case we didn't
	## write anything so don't increment %di.
	jc .Ldetect_mem_map__success

	add $E820_ENTRY_SZ, %di

	## We ran out of space in the e820_mem_map array. (The last
	## entry must be a null entry.)
	cmp $.Le820_mem_map__end, %di
	jge .Ldetect_mem_map__err

	## %ebx=0, which means we just wrote the last entry.
	test %ebx, %ebx
	jz .Ldetect_mem_map__success

	jmp .Ldetect_mem_map__loop

.Ldetect_mem_map__success:
	sub $e820_mem_map, %di
	mov %di, %ax
	mov $E820_ENTRY_SZ, %bl
	div %bl
	call printb		# Print result in %al

	mov $.Ldetect_mem_map_success_str, %di
	call println
	jmp .Ldetect_mem_map__exit

.Ldetect_mem_map__err:
	mov $.Ldetect_mem_map_err_str, %di
	call println

.Ldetect_mem_map__exit:
	pop %edx
	pop %ecx
	pop %ebx
	pop %eax
	pop %di
	ret

	.section .data

	## Memory map. Can have up to 31 24-byte entries (including
	## ones added later by the bootloader). The end of the list
	## will always be a null entry.
	##
	## 31 entries should be plenty. On QEMU we were only given 7
	## entries in the memory map.
	.align 8
e820_mem_map:
	.fill E820_MM_MAX_ENTRIES * E820_ENTRY_SZ
.Le820_mem_map__end:

	.section .rodata
	## Global symbol for C code.
e820_mm_max_entries:
	.long E820_MM_MAX_ENTRIES
.Ldetect_mem_map_err_str:
	.asciz "out of space in e820 array"
.Ldetect_mem_map_success_str:
	.asciz " memmap entries written"
