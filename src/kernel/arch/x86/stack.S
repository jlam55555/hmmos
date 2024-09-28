	.section .text

	.globl switch_stack
	.globl setup_stack

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
	mov 0xc(%esp), %ecx	# kthread
	mov 0x8(%esp), %eax	# thunk
	mov %esp, %edx		# save the current stack pointer

	# We're acting on the new stack now.
	mov 0x4(%esp), %esp

	# .Linit_thread(kthread, thunk)
	push %eax
	push %ecx
	push $.Linit_thread 	# return address for switch_stack

	push $0
	push $0
	push $0
	push $0
	mov %esp, %eax

	# Return to old stack.
	mov %edx, %esp
	ret

	## void .Linit_thread(KernelThread *kthread, void (*thunk)());
	##
	## Called at the start of each thread.
.Linit_thread:
	## on_thread_start(kthread, thunk)
	call on_thread_start

	pop %eax		# kthread
	pop %eax		# thunk
	push $0			# return address for `thunk`
	jmp *%eax
