#include "sched/kthread.h"
#include "memdefs.h"
#include "mm/kmalloc.h"
#include "nonstd/libc.h"
#include "perf.h"
#include "stack.h"
#include "timer.h"
#include "util/algorithm.h"
#include "util/assert.h"
#include <cstddef>

namespace {

/// Thread ID counter.
sched::ThreadID _tid = 0;

} // namespace

namespace sched {

KernelThread::KernelThread(Scheduler &_scheduler, void *_stack)
    : stack{_stack}, scheduler(_scheduler), tid(_tid++) {}

ThreadID Scheduler::bootstrap() {
  // Can't call `bootstrap()` on an already-running scheduler.
  ASSERT(running == nullptr);
  running = new KernelThread(*this);
  ASSERT(running != nullptr);

  // Maybe create dummy runnable task here so that something is always
  // schedulable. For now we assume that there is always a schedulable
  // thread, and `schedule()` will crash and burn if there isn't.

  return running->tid;
}

ThreadID Scheduler::new_thread(void (*thunk)()) {
  // Allocate a new stack, and set it up so that it looks like we've
  // just called into `switch_task()` with the return address being
  // the beginning of `f()`.

  // Note: the kernel heap implementation ensures that this is
  // page-aligned.
  void *stk = ::operator new(PG_SZ);
  ASSERT(stk != nullptr);

  // Go to the top of the stack.
  stk = (char *)stk + PG_SZ;

  auto *thread = new KernelThread(*this);
  thread->stack = arch::sched::setup_stack(stk, thunk, thread);

  // Add thread to the end of the round-robin space.
  runnable.push_back(*thread);

  return thread->tid;
}

const KernelThread *Scheduler::choose_task() const {
  ASSERT(running);

  // Round-robin from the runnable list.
  if (!runnable.empty()) {
    return &*runnable.begin();
  }

  // Runnable list is empty, check if the current task is runnable.
  if (running->runnable) {
    return running;
  }

  // Nothing to schedule.
  return nullptr;
}

void Scheduler::schedule(bool switch_stack) {
  ASSERT(running);

  KernelThread *new_task = const_cast<KernelThread *>(choose_task());
  KernelThread *current_task = running;

  // We should always be able to find a new task since there is a
  // dummy (always-schedulable) task.
  ASSERT(new_task);

  if (new_task == current_task) {
    // Nothing to do here.
    return;
  }

  // Do all the bookkeeping here, before we switch stacks. We can't do
  // this after switching stacks or else we'll be updating the
  // bookkeeping from the `new_task's` last context switch.
  running = new_task;
  new_task->erase();
  runnable.push_back(*current_task);
  context_switch_start = arch::time::rdtsc();

  // Unit tests aren't multithreaded, don't actually switch stacks but
  // do the rest of the bookkeeping.
  if (likely(switch_stack)) {
    arch::sched::switch_stack(&current_task->stack, new_task->stack);
  }

  // We've swapped stacks now.
  post_context_switch_bookkeeping();
}

void Scheduler::print_stats() const {
  // Note: runnable.size() is slow.
  nonstd::printf("scheduler stats:\r\n"
                 "\tcontext switches: %u\r\n"
                 "\tcycles/switch: %llu\r\n"
                 "\trunnable count: %u\r\n",
                 context_switch_count,
                 context_switch_cum_cycles / context_switch_count,
                 runnable.size());
}

void Scheduler::post_context_switch_bookkeeping() {
  context_switch_cum_cycles += arch::time::rdtsc() - context_switch_start;
  ++context_switch_count;

  if (unlikely(pending_deletion)) {
    delete_task(pending_deletion);
    pending_deletion = nullptr;
  }

  if (unlikely(context_switch_count % 100 == 0)) {
    print_stats();
  }
}

void Scheduler::destroy_thread(KernelThread *thread, bool switch_stack) {
  if (thread == nullptr) {
    thread = running;
  }

  ASSERT(thread);
  ASSERT(&thread->scheduler == this);

  bool destroy_current = thread == running;
  if (!destroy_current) {
    // If we're not cleaning up the current thread, the process is
    // relatively straightforward, and we can delete it right away.
    // We also don't have to schedule away right away.
    delete_task(thread);
  } else {
    // We can't safely deallocate the kernel thread and its
    // resources before scheduling away, so let's mark it for
    // deletion and clean it after the next call to `schedule()`.
    thread->runnable = false;

    // We should never have two threads pending deletion
    // simultaneously.
    ASSERT(pending_deletion == nullptr);
    pending_deletion = thread;

    schedule(switch_stack);
  }
}

void Scheduler::delete_task(KernelThread *thread) {
  ASSERT(thread);
  ASSERT(&thread->scheduler == this);

  thread->erase();

  // TODO: use std::byte* in more places rather than void*
  auto *stack_pg =
      (std::byte *)util::algorithm::floor_pow2<PG_SZ>((size_t)thread->stack);
  delete stack_pg;
  delete thread;
}

extern "C" {
void on_thread_start(KernelThread *thread, void (*thunk)()) {
  ASSERT(thread);
  thread->scheduler.post_context_switch_bookkeeping();
}
}

} // namespace sched
