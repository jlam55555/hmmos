#pragma once

/// \file
/// \brief Some asm functions to help perform thread switching.

namespace sched {
struct KernelThread;
}

namespace arch::sched {
extern "C" {

/// Switch stacks. This saves the old tasks's state (callee-save
/// registers) on its stack, and restores the new task's state from
/// its stack.
///
/// \a new_stk should either be set up from a previous call to \ref
/// switch_stack() or from \ref setup_stack(). For this reason, it is
/// not safe for \a new_stk to be the currently running thread.
void switch_stack(void **old_stk, void *new_stk);

/// Set up a stack that looks like a process that was scheduled-away
/// from in \ref switch_stack().
///
/// \return the stack with the added stack frame
void *setup_stack(void *stk, void (*thunk)(), ::sched::KernelThread *kthread);
}
} // namespace arch::sched
