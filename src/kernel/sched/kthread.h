#pragma once

/// \file
/// \brief Kernel thread implementation
///
/// Each kernel thread gets a 4KB stack.
///
/// TODO: more description
/// TODO: pre-emptive scheduling (timer interrupt) and synchronization
/// primitives

#include "util/intrusive_list.h"

namespace sched {

class Scheduler;
struct KernelThread : public util::IntrusiveListHead<KernelThread> {
  KernelThread(Scheduler &, void *stack = nullptr);

  void *stack;
  Scheduler &scheduler;
  bool runnable = true;

  /// Thread ID; will be automatically assigned.
  unsigned tid;
};

extern "C" {
/// This is a free function with C linkage as it is called via asm
/// code. This code is injected at each thread's startup so
/// bookkeeping tasks can be done (e.g., recording context switch
/// times).
///
/// This shouldn't need to do anything with the thunk, it's just
/// provided since it's conveniently available.
void on_thread_start(KernelThread *, void (*thunk)());
}

class Scheduler {
public:
  /// Enter the scheduler. This should be called in live but doesn't
  /// need to be called when unit-testing scheduler functionality.
  void bootstrap();

  /// \brief Select the next runnable process in round-robin order, and
  /// actually switch stacks.
  ///
  /// This will throw if there are no schedulable tasks remaining.
  void schedule();

  /// Create a new thread that will start execution at the given
  /// thunk.
  void new_thread(void (*thunk)());

  /// Print scheduler stats, for debugging purposes.
  void print_stats() const;

  /// \brief Destroy a thread.
  ///
  /// If the provided thread is not the running thread, then it is
  /// destroyed immediately. If it is the running thread, then we
  /// schedule away and defer the deletion until after scheduling
  /// away.
  ///
  /// Note that there are no safeguards against destroying all
  /// runnable threads such that there are no schedulable threads
  /// remaining.
  ///
  /// \param thread the thread to destroy, or nullptr to destroy the
  /// currently-running thread.
  void destroy_thread(KernelThread *thread = nullptr);

private:
  KernelThread *running = nullptr;
  util::IntrusiveListHead<KernelThread> runnable;
  util::IntrusiveListHead<KernelThread> blocked;

  KernelThread *pending_deletion = nullptr;

  /// \brief Returns a runnable task to schedule next.
  ///
  /// \return thread to schedule. This should be non-null if there is
  /// a dummy (always-schedulable) thread present.
  const KernelThread *choose_task() const;

  /// \brief Post-context switch bookkeeping actions.
  ///
  /// These are performed after switching stacks to the new task, so
  /// they are performed in the context of the new task. However, this
  /// should only update bookkeeping for the scheduler as a whole --
  /// any bookkeeping specific to the context switch should have been
  /// done _before_ the context switch.
  ///
  /// Currently, this performs the following tasks:
  /// - Updates \ref context_switch_count and \ref
  ///   context_switch_cum_cycles.
  /// - Performs any pending thread deletions (since we can't delete
  ///   the thread's stack+descriptor until after we've switched away
  ///   from it.)
  /// - Periodically print scheduler stats.
  void post_context_switch_bookkeeping();

  /// \brief Delete a thread that's not in use.
  ///
  /// This is the internal logic to free all the necessary data
  /// structures; external clients should call `destroy_thread()` to
  /// safely delete a task.
  void delete_task(KernelThread *thread);

  /// \brief Number of calls to `schedule()`.
  ///
  /// Currently, this doesn't count context switches from a function
  /// to itself. This does count context switches to newly-created
  /// threads.
  unsigned context_switch_count = 0;

  /// \brief Cumulative cycles (as counted by TSC) for all context
  /// switches.
  ///
  /// This currently only counts the cycles spent in the stack switch,
  /// for all context switches counted by \ref context_switch_count.
  uint64_t context_switch_cum_cycles = 0;
  uint64_t context_switch_start;

  friend void ::sched::on_thread_start(KernelThread *, void (*)());
};

} // namespace sched
