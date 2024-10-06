#include "../test.h"
#include "sched/kthread.h"

/// @file Test scheduler round-robin selection.
///
/// This file doesn't test the actual stack-switching behavior, which
/// is is only a few lines and is exercised on each context switch.

namespace sched {
class TestScheduler : public Scheduler {
public:
  unsigned num_threads() const {
    return !!running + runnable.size() + blocked.size();
  }

  ThreadID get_running_tid() const {
    return running ? running->tid : bad_thread;
  }

  ThreadID choose_task_tid() const {
    if (auto thread = choose_task()) {
      return thread->tid;
    }
    return bad_thread;
  }
};
} // namespace sched

TEST_CLASS(sched, Scheduler, one_runnable_thread) {
  TestScheduler scheduler;
  unsigned tid0 = scheduler.bootstrap();

  TEST_ASSERT(scheduler.num_threads() == 1);
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  TEST_ASSERT(scheduler.choose_task_tid() == tid0);

  scheduler.schedule(false);

  TEST_ASSERT(scheduler.num_threads() == 1);
  TEST_ASSERT(scheduler.get_running_tid() == tid0);
  TEST_ASSERT(scheduler.choose_task_tid() == tid0);
}

// NOCOMMIT:
// TODO: multiple runnable threads (including some blocked threads)
// TODO: round-robin
// TODO: round-robin with deletes/adds
