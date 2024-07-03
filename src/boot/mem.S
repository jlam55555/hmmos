	## Detect memory map using the BIOS 0xE820 function.
	.code16
	.text

	.globl detect_mem_map
	.globl e820_mem_map

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
	mov $0x534D4150, %edx	# Magic number.

.Ldetect_mem_map__loop:
	mov $0x0000E820, %eax
	mov $24, %ecx
	int $0x15

	## CF is set after the last entry. In this case we didn't
	## write anything so don't increment %di.
	jc .Ldetect_mem_map__success

	add $24, %di

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
	mov $24, %bl
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

	## Memory map. Will have up to 15 24-byte entries. The end of
	## the list will always be a null entry.
	.align 8
e820_mem_map:
	.fill 16 * 24
.Le820_mem_map__end:

	.section .rodata
.Ldetect_mem_map_err_str:
	.asciz "out of space in e820 array"
.Ldetect_mem_map_success_str:
	.asciz " memmap entries written"
