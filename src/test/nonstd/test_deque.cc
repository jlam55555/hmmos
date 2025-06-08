#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/deque.h"
#include "nonstd/string_view.h"

template <typename T> using rcdeque = RCContainer<nonstd::deque, T>;

TEST_CLASS_WITH_FIXTURE(nonstd, deque, basic, LeakChecker) {
  const rcdeque<int> d{1, 2, 3};
  TEST_ASSERT(d.front() == 1);
  TEST_ASSERT(d[0] == 1);
  TEST_ASSERT(d[1] == 2);
  TEST_ASSERT(d[2] == 3);
  TEST_ASSERT(d.back() == 3);
  TEST_ASSERT(!d.empty());
  TEST_ASSERT(d.size() == 3);
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, constructor, LeakChecker) {
  TEST_ASSERT(rcdeque<int>{}.empty());
  TEST_ASSERT(rcdeque<int>(3) == rcdeque<int>{0, 0, 0});
  TEST_ASSERT(rcdeque<int>(3, 5) == rcdeque<int>{5, 5, 5});

  constexpr auto s = "hello"_sv;
  TEST_ASSERT(rcdeque<char>(s.begin(), s.end()) ==
              rcdeque<char>{'h', 'e', 'l', 'l', 'o'});

  const rcdeque<int> d{1, 2, 3};
  TEST_ASSERT(rcdeque<int>(d) == d);
  TEST_ASSERT(rcdeque<int>(rcdeque<int>{1, 2, 3}) == d);
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, assignment, LeakChecker) {
  {
    // Non-empty -> non-empty
    rcdeque<int> d1{1, 2, 3};
    const rcdeque<int> d2{4, 5, 6};
    d1 = d2;
    TEST_ASSERT(d1 == d2);
    TEST_ASSERT(d1 == rcdeque<int>{4, 5, 6});

    d1 = rcdeque<int>{7, 8, 9};
    TEST_ASSERT(d1 == rcdeque<int>{7, 8, 9});
  }

  {
    // Non-empty -> empty
    rcdeque<int> d1{1, 2, 3};
    d1 = rcdeque<int>{};
    TEST_ASSERT(d1.empty());
  }

  {
    // Empty -> non-empty
    rcdeque<int> d1;
    d1 = rcdeque<int>{2, 3, 4};
    TEST_ASSERT(d1 == rcdeque<int>{2, 3, 4});
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, element_access, LeakChecker) {
  {
    // const
    const rcdeque<int> d{1, 2, 3, 4, 5};
    TEST_ASSERT(d.at(0) == 1);
    TEST_ASSERT(d.at(4) == 5);
    TEST_ASSERT(d[0] == 1);
    TEST_ASSERT(d[4] == 5);
    TEST_ASSERT(d.front() == 1);
    TEST_ASSERT(d.back() == 5);
  }

  {
    // non-const
    rcdeque<int> d{1, 2, 3};
    d.front() = 5;
    d[1] = 6;
    d.back() = 7;
    TEST_ASSERT(d == rcdeque<int>{5, 6, 7});
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, iterators, LeakChecker) {
  const rcdeque<int> d{1, 2, 3};
  TEST_ASSERT(rcdeque<int>(d.begin(), d.end()) == d);
  TEST_ASSERT(rcdeque<int>(d.rbegin(), d.rend()) == rcdeque<int>{3, 2, 1});

  TEST_ASSERT(*d.begin() == d.front());
  TEST_ASSERT(*std::prev(d.end()) == d.back());
  TEST_ASSERT(*d.rbegin() == d.back());
  TEST_ASSERT(*std::prev(d.rend()) == d.front());
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, capacity, LeakChecker) {
  TEST_ASSERT(rcdeque<int>{}.empty());
  TEST_ASSERT(rcdeque<int>{}.size() == 0);
  TEST_ASSERT(!rcdeque<int>{1, 2, 3}.empty());
  TEST_ASSERT(rcdeque<int>{1, 2, 3}.size() == 3);

  {
    rcdeque<int> d;
    TEST_ASSERT(d.empty());
    d.shrink_to_fit();
    TEST_ASSERT(d.empty());
    for (int i = 0; i < 64; ++i) {
      d.push_front(-i - 1);
      d.push_back(i);
    }
    auto it = d.begin();
    for (int i = -64; i < 64; ++i) {
      TEST_ASSERT(*it++ == i);
    }
    d.clear();
    d.shrink_to_fit();
    // It's hard to test that shrink_to_fit() DTRT from this test, but
    // you can put some logging statements into the implementation to
    // see that blocks are freed correctly.
    TEST_ASSERT(d.empty());
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, modifiers, LeakChecker) {
  {
    rcdeque<int> d{1, 2, 3};
    TEST_ASSERT(!d.empty());
    d.clear();
    TEST_ASSERT(d.empty());
  }

  {
    // The current implementation only allows insert/erase at the
    // front/back. Not because of any technical limitation, but
    // ... I'm lazy.
    rcdeque<int> d{1, 2, 3};
    d.insert(d.begin(), 4);
    TEST_ASSERT(d == rcdeque<int>{4, 1, 2, 3});
    d.insert(d.end(), 2, 5);
    TEST_ASSERT(d == rcdeque<int>{4, 1, 2, 3, 5, 5});

    // Note: inserting multiple elements at the front is not supported
    // for now.
    d.insert(d.end(), {7, 8, 9});
    TEST_ASSERT(d == rcdeque<int>{4, 1, 2, 3, 5, 5, 7, 8, 9});
  }

  {
    rcdeque<int> d{1, 2, 3};
    d.emplace(d.begin(), 4);
    TEST_ASSERT(d == rcdeque<int>{4, 1, 2, 3});
    d.emplace(d.end(), 5);
    TEST_ASSERT(d == rcdeque<int>{4, 1, 2, 3, 5});
  }

  {
    rcdeque<int> d{1, 2, 3, 4, 5};
    d.erase(d.begin());
    TEST_ASSERT(d == rcdeque<int>{2, 3, 4, 5});
    d.erase(std::prev(d.end()));
    TEST_ASSERT(d == rcdeque<int>{2, 3, 4});
    d.erase(d.begin(), std::prev(d.end()));
    TEST_ASSERT(d == rcdeque<int>{4});
  }

  {
    rcdeque<int> d{1, 2, 3};
    d.push_back(4);
    TEST_ASSERT(d == rcdeque<int>{1, 2, 3, 4});
    d.pop_back();
    TEST_ASSERT(d == rcdeque<int>{1, 2, 3});
    d.push_front(5);
    TEST_ASSERT(d == rcdeque<int>{5, 1, 2, 3});
    d.pop_front();
    TEST_ASSERT(d == rcdeque<int>{1, 2, 3});
  }

  {
    rcdeque<int> d{1, 2, 3};
    d.resize(5);
    TEST_ASSERT(d == rcdeque<int>{1, 2, 3, int{}, int{}});
    d.resize(2);
    TEST_ASSERT(d == rcdeque<int>{1, 2});
    d.resize(5, -1);
    TEST_ASSERT(d == rcdeque<int>{1, 2, -1, -1, -1});
    d.resize(1, -1);
    TEST_ASSERT(d == rcdeque<int>{1});
  }

  {
    rcdeque<int> d1{1, 2, 3};
    rcdeque<int> d2{4, 5, 6};
    d1.swap(d2);
    TEST_ASSERT(d1 == rcdeque<int>{4, 5, 6});
    TEST_ASSERT(d2 == rcdeque<int>{1, 2, 3});
    d1.clear();
    d2.swap(d1);
    TEST_ASSERT(d1 == rcdeque<int>{1, 2, 3});
    TEST_ASSERT(d2.empty());
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, deque, complex, LeakChecker) {
  // Test a complicated set of insertions/deletions, especially with
  // insertions to the front and forcing a resize of the internal
  // array.
  //
  // These constants are copied from the deque definition, we should
  // update these if they change.
  constexpr size_t elems_per_block = 512 / sizeof(int);
  constexpr size_t min_block_count = 16;

  // Push both to the front and back until we've saturated all blocks
  // in the internal block.
  rcdeque<int> d;
  const size_t count = elems_per_block * min_block_count;
  for (size_t i = 0; i < count; ++i) {
    if (i & 1) {
      d.push_back(i);
    } else {
      d.push_front(i);
    }
  }
  TEST_ASSERT(d.size() == count);
  // ...4,2,0,1,3,5...
  for (size_t i = 0; i < count; ++i) {
    if (i & 1) {
      TEST_ASSERT(d[(count + i) / 2] == i);
    } else {
      TEST_ASSERT(d[(count - i - 1) / 2] == i);
    }
  }

  // Push us over the edge. This should force a reallocation but the
  // contents should be identical.
  rcdeque<int> d2 = d;
  d2.push_front(-1);
  TEST_ASSERT(d2.size() == count + 1);
  TEST_ASSERT(d2.front() == -1);
  for (size_t i = 0; i < count; ++i) {
    TEST_ASSERT(d2[i + 1] == d[i]);
  }

  // Erase enough elements to have free blocks. Shrink to fit and show
  // that it is still value-equal, even if the internal
  // representations of the two deques have different numbers of
  // blocks.
  for (size_t i = 0; i < count / 2; ++i) {
    if (i & 1) {
      d.pop_back();
    } else {
      d.pop_front();
    }
  }
  TEST_ASSERT(d.size() == count / 2);
  rcdeque<int> d3;
  d3.insert(d3.end(), d.begin(), d.end());
  d.shrink_to_fit();
  TEST_ASSERT(d == d3);
}
