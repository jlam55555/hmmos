	.section .text

	.globl switch_stack
	.globl setup_stack
	.globl enter_userspace

switch_stack:
	mov 0x8(%esp), %ecx	# new_stk
	mov 0x4(%esp), %eax 	# &old_stk

	# Old stack
	push %ebx
	push %ebp
	push %edi
	push %esi
	mov %esp, (%eax)

	# New stack
	mov %ecx, %esp
	pop %esi
	pop %edi
	pop %ebp
	pop %ebx
	ret

setup_stack:
	mov %esp, %edx		# save the current stack pointer

	# We're acting on the new stack now.
	mov 0x4(%esp), %esp 	# new_stk

	# .Linit_thread(kthread, fcn, data)
	push 0x10(%edx)		# data
	push 0xc(%edx)		# fcn
	push 0x8(%edx)		# kthread
	push $.Linit_thread 	# return address for switch_stack

	push $0			# %ebx
	push $0			# %ebp
	push $0			# %edi
	push $0			# %esi
	mov %esp, %eax

	# Return to old stack.
	mov %edx, %esp
	ret

	## void .Linit_thread(KernelThread *kthread,
	##                    void (*fcn)(void*), void *data);
	##
	## Called at the start of each thread.
.Linit_thread:
	## on_thread_start(kthread, fcn, data)
	call on_thread_start

	pop %eax		# kthread
	pop %eax		# fcn
	# Note: \a data is still on the stack here as the first argument.
	push $0			# return address for `fcn`
	jmp *%eax		# call `fcn`

enter_userspace:
	mov $0x23, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %esp, %edx

	push $0x23		# ss3
	push 0x4(%edx)		# esp3
	pushf			# eflags3
	push $0x1B		# cs3
	push 0x8(%edx)		# eip3

	xor %eax, %eax
	xor %ebx, %ebx
	xor %ecx, %ecx
	xor %edx, %edx
	xor %edi, %edi
	xor %esi, %esi
	iret
