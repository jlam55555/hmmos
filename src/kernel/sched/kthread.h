#pragma once

/// \file
/// \brief Kernel thread and thread scheduler implementation
///
/// The general interface for the scheduler is:
/// - `Scheduler::bootstrap()`: register the current thread in the
///   scheduler.
/// - `Scheduler::new_thread(fcn)`: add a new task that will start
///   executing `fcn`. This will be given a 4KB stack.
/// - `Scheduler::destroy_thread()`: destroy the current running
///   thread, and schedule away.
/// - `Scheduler::schedule()`: context-switch to the next runnable
///   task in a round-robin manner.
///
/// TODO: pre-emptive scheduling (timer interrupt) and synchronization
/// primitives

#include "nonstd/node_hash_map.h"
#include "util/assert.h"
#include "util/intrusive_list.h"

namespace proc {
class Process;
}

namespace sched {

class Scheduler;
class TestScheduler;
class KernelThread;

extern "C" {
/// This is a free function with C linkage as it is called via asm
/// code. This code is injected at each thread's startup so
/// bookkeeping tasks can be done (e.g., recording context switch
/// times).
///
/// This shouldn't need to do anything with the fcn, it's just
/// provided since it's conveniently available.
void on_thread_start(KernelThread *, void (*fcn)(void *), void *data);
}

using ThreadID = uint16_t;
static constexpr ThreadID InvalidTID = -1;

/// \brief Internal representation of a kernel thread.
///
/// This is an internal data structure; threads can be identified by
/// TID. This only need be defined in the header for the intrusive
/// list head in Scheduler; External clients do not have any way to
/// construct/get access to a valid KernelThread object via the \ref
/// Scheduler interface..
class KernelThread final : public util::IntrusiveListHead<KernelThread> {
private:
  KernelThread(Scheduler &, void *stack = nullptr);

  void *stack;
  Scheduler &scheduler;
  bool runnable = true;

  /// Thread ID; will be automatically assigned and unique over the
  /// lifetime of thread.
  ThreadID tid;

  /// Process that this thread is associated with. Can be nullptr if
  /// this is a purely kernel thread.
  proc::Process *proc;

  friend class Scheduler;
  friend void on_thread_start(KernelThread *, void (*)(void *), void *data);
  friend class TestScheduler;
};

class Scheduler {
public:
  Scheduler();

  /// Enter the scheduler. This should be called in live but doesn't
  /// need to be called when unit-testing scheduler functionality.
  ThreadID bootstrap();

  /// \brief Select the next runnable process in round-robin order, and
  /// actually switch stacks.
  ///
  /// This will throw if there are no schedulable tasks remaining.
  void schedule() { return schedule(/*switch_stack=*/true); }

  /// Create a new thread that will start execution at the given
  /// fcn. \a proc can be nullptr if this thread is not associated
  /// with a userspace process.
  ThreadID new_thread(proc::Process *proc, void (*fcn)(void *), void *data);

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
  void destroy_thread(ThreadID tid) {
    auto it = tid_map.find(tid);
    ASSERT(it != tid_map.end() && it->second != nullptr);
    return destroy_thread(/*thread=*/it->second, /*switch_stack=*/true);
  }

  proc::Process *curr_proc() const {
    return curr_proc_override ? curr_proc_override
                              : (running ? running->proc : nullptr);
  }

  /// Ugly hack that should only be used in the \ref proc::Process
  /// constructor. We want to run kernel code from the context of the
  /// newly created process, but this code should be run immediately
  /// from the parent process.
  void override_curr_proc(proc::Process *proc) {
    ASSERT(implies(proc != nullptr, curr_proc_override == nullptr));
    curr_proc_override = proc;
  }

private:
  // For unit testing
  friend class TestScheduler;

  KernelThread *running = nullptr;
  util::IntrusiveListHead<KernelThread> runnable;
  util::IntrusiveListHead<KernelThread> blocked;

  KernelThread *pending_deletion = nullptr;

  /// \brief Private implementation for unit testing.
  /// \sa schedule()
  /// \param switch_stack Should only be set to false in a unit test,
  /// where we don't actually have multithreading.
  void schedule(bool switch_stack);

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

  /// \sa schedule()
  void destroy_thread(KernelThread *thread, bool switch_stack);

  /// \brief Delete a thread that's not in use.
  ///
  /// This is the internal logic to free all the necessary data
  /// structures for a thread that isn't currently-scheduled; external
  /// clients should call `destroy_thread()` to safely delete a task.
  void delete_task(KernelThread *thread);

  /// \brief Assigns the next ThreadID that's not in use to the given
  /// thread.
  ///
  /// This will throw if there are no free thread IDs.
  void assign_next_tid(KernelThread *new_thread);

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

  /// Thread ID management.
  ThreadID tid_counter = 0;
  nonstd::node_hash_map<ThreadID, sched::KernelThread *> tid_map;

  /// \see \ref override_curr_proc().
  proc::Process *curr_proc_override = nullptr;

  friend void ::sched::on_thread_start(KernelThread *, void (*)(void *),
                                       void *data);
};

} // namespace sched
