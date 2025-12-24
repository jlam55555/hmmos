#include "../test.h"
#include "sched/kthread.h"

/// \file Test scheduler round-robin selection.
///
/// This file doesn't test the actual stack-switching behavior, which
/// is is only a few lines and is exercised on each context switch.

namespace sched {
class TestScheduler : public Scheduler {
public:
  void schedule() { return Scheduler::schedule(/*switch_stack=*/false); }
  void destroy_thread() { return Scheduler::destroy_thread(nullptr, false); }

  unsigned num_threads() const {
    return !!running + runnable.size() + blocked.size();
  }

  ThreadID get_running_tid() const {
    return running ? running->tid : InvalidTID;
  }

  ThreadID choose_task_tid() const {
    if (auto thread = choose_task()) {
      return thread->tid;
    }
    return InvalidTID;
  }
};
} // namespace sched

TEST_CLASS(sched, Scheduler, one_runnable_thread) {
  TestScheduler scheduler;
  ThreadID tid0 = scheduler.bootstrap();

  TEST_ASSERT(scheduler.num_threads() == 1);
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  TEST_ASSERT(scheduler.choose_task_tid() == tid0);

  scheduler.schedule();

  TEST_ASSERT(scheduler.num_threads() == 1);
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  TEST_ASSERT(scheduler.choose_task_tid() == tid0);
}

TEST_CLASS(sched, Scheduler, round_robin) {
  TestScheduler scheduler;
  ThreadID tid0 = scheduler.bootstrap();
  ThreadID tid1 = scheduler.new_thread(nullptr, nullptr, nullptr);
  ThreadID tid2 = scheduler.new_thread(nullptr, nullptr, nullptr);
  ThreadID tid3 = scheduler.new_thread(nullptr, nullptr, nullptr);

  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid1);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid2);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid3);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid1);

  // Delete a thread, see that it doesn't get scheduled anymore. The
  // external interface only currently supports destroying the current
  // thread.
  scheduler.destroy_thread();
  TEST_ASSERT(scheduler.get_running_tid() == tid2);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid3);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid2);

  // Create a new thread, see that it gets added to the end of the
  // round-robin order.
  ThreadID tid4 = scheduler.new_thread(nullptr, nullptr, nullptr);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid3);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid4);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid2);
  scheduler.schedule();
  TEST_ASSERT(scheduler.get_running_tid() == tid3);
}

// TODO: unit test scheduling with blocked threads. Do this once we
// actually set threads as blocked.
