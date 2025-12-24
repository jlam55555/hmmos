#pragma once

/// \file lock.h
/// \brief Locking primitives
///
/// TODO: when supporting SMP, we need spinlocks
/// TODO: we also need mutex (non-blocking) primitives
/// TODO: we also need condvar/semaphore signaling primitives

namespace sched {

/// Mutually exclusive lock in a uniprocessor system.
///
/// This just disables interrupts.
__attribute__((always_inline)) inline void mutex_lock() {
  __asm__ volatile("cli");
}

/// Unlock interrupts after \ref lock() in a uniprocessor system.
__attribute__((always_inline)) inline void mutex_unlock() {
  __asm__ volatile("sti");
}

} // namespace sched
