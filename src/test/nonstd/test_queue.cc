#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/queue.h"
#include "nonstd/string.h"
#include <utility>

template <typename T> using rcqueue = RCContainer<nonstd::queue, T>;

TEST_CLASS_WITH_FIXTURE(nonstd, queue, constructor, LeakChecker) {
  TEST_ASSERT(rcqueue<int>{}.empty());

  const string s = "abc";
  const rcqueue<char> q{s.begin(), s.end()};
  TEST_ASSERT(rcqueue<char>{q} == q);
  TEST_ASSERT(rcqueue<char>{rcqueue<char>{q}} == q);
}

TEST_CLASS_WITH_FIXTURE(nonstd, queue, element_access, LeakChecker) {
  const string s = "abc";
  const rcqueue<char> q{s.begin(), s.end()};
  TEST_ASSERT(s.front() == 'a');
  TEST_ASSERT(s.back() == 'c');

  rcqueue<char> mutable_q{q};
  mutable_q.back() = 'd';
  TEST_ASSERT(mutable_q.back() == 'd');
  mutable_q.pop();
  mutable_q.pop();
  TEST_ASSERT(mutable_q.front() == 'd');
}

TEST_CLASS_WITH_FIXTURE(nonstd, queue, capacity, LeakChecker) {
  rcqueue<int> q;
  TEST_ASSERT(q.empty());
  TEST_ASSERT(q.size() == 0);

  q.push(1);
  q.push(2);
  TEST_ASSERT(!q.empty());
  TEST_ASSERT(q.size() == 2);
}

TEST_CLASS_WITH_FIXTURE(nonstd, queue, modifiers, LeakChecker) {
  rcqueue<int> q;
  for (int i = 0; i < 10; ++i) {
    q.push(i);
    TEST_ASSERT(q.back() == i);
  }
  TEST_ASSERT(q.size() == 10);
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT(q.front() == i);
    q.pop();
  }
  TEST_ASSERT(q.empty());
}
