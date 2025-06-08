#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/stack.h"
#include "nonstd/string.h"

template <typename T> using rcstack = RCContainer<nonstd::stack, T>;

TEST_CLASS_WITH_FIXTURE(nonstd, stack, constructor, LeakChecker) {
  TEST_ASSERT(rcstack<int>{}.empty());

  const string s = "abc";
  const rcstack<char> stk{s.begin(), s.end()};
  TEST_ASSERT(rcstack<char>{stk} == stk);
  TEST_ASSERT(rcstack<char>{rcstack<char>{stk}} == stk);
}

TEST_CLASS_WITH_FIXTURE(nonstd, stack, element_access, LeakChecker) {
  const string s = "abc";
  const rcstack<char> stk{s.begin(), s.end()};
  TEST_ASSERT(stk.top() == 'c');

  rcstack<char> mutable_stk{stk};
  mutable_stk.top() = 'd';
  TEST_ASSERT(mutable_stk.top() == 'd');
  mutable_stk.pop();
  TEST_ASSERT(mutable_stk.top() == 'b');
  mutable_stk.pop();
  TEST_ASSERT(mutable_stk.top() == 'a');
  mutable_stk.pop();
  TEST_ASSERT(mutable_stk.empty());
}

TEST_CLASS_WITH_FIXTURE(nonstd, stack, capacity, LeakChecker) {
  rcstack<int> stk;
  TEST_ASSERT(stk.empty());
  TEST_ASSERT(stk.size() == 0);

  stk.push(1);
  stk.push(2);
  TEST_ASSERT(!stk.empty());
  TEST_ASSERT(stk.size() == 2);
}

TEST_CLASS_WITH_FIXTURE(nonstd, stack, modifiers, LeakChecker) {
  rcstack<int> stk;
  for (int i = 0; i < 10; ++i) {
    stk.push(i);
    TEST_ASSERT(stk.top() == i);
  }
  TEST_ASSERT(stk.size() == 10);
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT(stk.top() == 9 - i);
    stk.pop();
  }
  TEST_ASSERT(stk.empty());
}
