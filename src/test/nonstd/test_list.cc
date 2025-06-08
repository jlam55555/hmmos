#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/list.h"
#include "nonstd/string.h"
#include "nonstd/vector.h"

template <typename T> using rclist = RCContainer<nonstd::list, T>;

TEST_CLASS_WITH_FIXTURE(nonstd, list, constructor, LeakChecker) {
  TEST_ASSERT(rclist<int>{}.empty());
  TEST_ASSERT(rclist<int>(3) == rclist<int>{0, 0, 0});
  TEST_ASSERT(rclist<int>(3, 2) == rclist<int>{2, 2, 2});
  string s{"foobar"};
  TEST_ASSERT(rclist<char>(s.begin(), s.end()) ==
              rclist<char>{'f', 'o', 'o', 'b', 'a', 'r'});
  rclist<int> l{1, 2, 3};
  TEST_ASSERT(rclist<int>(l) == l);
  TEST_ASSERT(rclist<int>(rclist<int>{1, 2, 3}) == l);
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, assignment, LeakChecker) {
  rclist<int> l1;
  const rclist<int> l2{1, 2, 3};
  l1 = l2;
  TEST_ASSERT(l1 == l2);
  TEST_ASSERT(l1 == rclist<int>{1, 2, 3});

  l1 = rclist<int>{4, 5, 6};
  TEST_ASSERT(l1 == rclist<int>{4, 5, 6});

  // Ensure that assigned vectors don't get linked to each other
  // (because the initial implementation had bugs like this).
  rclist<int> l3, l4;
  l3 = l4;
  l4 = l3;
  TEST_ASSERT(l3 == l4);
  l3.push_back(1);
  l4.push_back(2);
  TEST_ASSERT(l3 == rclist<int>{1});
  TEST_ASSERT(l4 == rclist<int>{2});
  l4 = l3;
  TEST_ASSERT(l3 == l4);
  l3.pop_front();
  TEST_ASSERT(l3 != l4);
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, element_access, LeakChecker) {
  TEST_ASSERT(rclist<int>{1, 2, 3}.front() == 1);
  TEST_ASSERT(rclist<int>{1, 2, 3}.back() == 3);
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, iterators, LeakChecker) {
  rclist<int> l{1, 2, 3};
  TEST_ASSERT(rclist<int>(l.begin(), l.end()) == l);
  TEST_ASSERT(rclist<int>(l.rbegin(), l.rend()) == rclist<int>{3, 2, 1});

  TEST_ASSERT(*l.begin() == l.front());
  TEST_ASSERT(*std::prev(l.end()) == l.back());
  TEST_ASSERT(*l.rbegin() == l.back());
  TEST_ASSERT(*std::prev(l.rend()) == l.front());

  const rclist<int> l2;
  TEST_ASSERT(l2.begin() == l2.end());
  TEST_ASSERT(l2.rbegin() == l2.rend());
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, capacity, LeakChecker) {
  TEST_ASSERT(rclist<int>{}.empty());
  TEST_ASSERT(rclist<int>{}.size() == 0);
  TEST_ASSERT(!rclist<int>{1, 2, 3}.empty());
  TEST_ASSERT(rclist<int>{1, 2, 3}.size() == 3);
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, modifiers, LeakChecker) {
  rclist<int> l1{1, 2, 3};
  TEST_ASSERT(!l1.empty());
  l1.clear();
  TEST_ASSERT(l1.empty());
  l1.clear();
  TEST_ASSERT(l1.empty());

  rclist<int> l2{1, 2, 3};
  auto it1 = l2.insert(l2.end(), 4);
  TEST_ASSERT(l2 == rclist<int>{1, 2, 3, 4});
  TEST_ASSERT(std::next(it1) == l2.end());
  auto it2 = l2.insert(it1, 5);
  TEST_ASSERT(l2 == rclist<int>{1, 2, 3, 5, 4});
  TEST_ASSERT(std::next(it2, 2) == l2.end());
  auto it3 = l2.insert(l2.begin(), 6);
  TEST_ASSERT(l2 == rclist<int>{6, 1, 2, 3, 5, 4});
  TEST_ASSERT(it3 == l2.begin());

  auto it4 = l2.insert(std::next(l2.begin()), 3, 7);
  TEST_ASSERT(l2 == rclist<int>{6, 7, 7, 7, 1, 2, 3, 5, 4});
  TEST_ASSERT(std::prev(it4) == l2.begin());

  rclist<int> l3;
  auto it5 =
      l3.insert(l3.begin(), std::next(l2.rbegin()), std::prev(l2.rend()));
  TEST_ASSERT(l3 == rclist<int>{5, 3, 2, 1, 7, 7, 7});
  TEST_ASSERT(it5 == l3.begin());

  l3.insert(l3.end(), {8, 9});
  TEST_ASSERT(l3 == rclist<int>{5, 3, 2, 1, 7, 7, 7, 8, 9});

  using rcli = rclist<int>;
  rclist<rclist<int>> l4;
  l4.emplace(l4.end(), std::initializer_list<RCWrapper<int>>{1, 2, 3});
  l4.emplace(l4.end(), 3, 4);
  TEST_ASSERT(l4 == rclist<rcli>{rcli{1, 2, 3}, rcli{4, 4, 4}});

  // erase the middle element of the first sublist of l4
  auto it6 = l4.front().erase(std::prev(l4.front().end(), 2));
  TEST_ASSERT(l4 == rclist<rcli>{rcli{1, 3}, rcli{4, 4, 4}});
  TEST_ASSERT(*it6 == 3);

  auto it7 = l4.erase(std::next(l4.begin()));
  TEST_ASSERT(l4.size() == 1);
  TEST_ASSERT(it7 == l4.end());

  l4.push_back(rclist<int>{5, 6});
  TEST_ASSERT(l4 == rclist<rcli>{rcli{1, 3}, rcli{5, 6}});

  vector<int> v{9, 8, 7};
  l4.emplace_back(v.begin(), v.end());
  TEST_ASSERT(l4 == rclist<rcli>{rcli{1, 3}, rcli{5, 6}, rcli{9, 8, 7}});

  l4.pop_back();
  TEST_ASSERT(l4 == rclist<rcli>{rcli{1, 3}, rcli{5, 6}});

  l4.push_front(rcli{});
  TEST_ASSERT(l4 == rclist<rcli>{rcli{}, rcli{1, 3}, rcli{5, 6}});

  l4.emplace_front(v.begin(), v.end());
  TEST_ASSERT(l4 ==
              rclist<rcli>{rcli{9, 8, 7}, rcli{}, rcli{1, 3}, rcli{5, 6}});

  l4.pop_front();
  TEST_ASSERT(l4 == rclist<rcli>{rcli{}, rcli{1, 3}, rcli{5, 6}});
  l4.pop_front();
  l4.pop_front();
  l4.pop_front();
  TEST_ASSERT(l4.empty());
}

TEST_CLASS_WITH_FIXTURE(nonstd, list, operations, LeakChecker) {
  {
    rclist<int> l1{1, 2};
    rclist<int> l2{3, 4};
    l1.splice(l1.begin(), l2);
    TEST_ASSERT(l1 == rclist<int>{3, 4, 1, 2});
    TEST_ASSERT(l2.empty());

    l1.splice(std::prev(l1.end()), rclist<int>{5, 6});
    TEST_ASSERT(l1 == rclist<int>{3, 4, 1, 5, 6, 2});
  }

  {
    // Splicing from an empty list.
    rclist<int> l1{1, 2};
    rclist<int> l2;
    l1.splice(l1.end(), l2);
    TEST_ASSERT(l1 == rclist<int>{1, 2});
    TEST_ASSERT(l2.empty());
    l1.splice(l1.end(), {});
    TEST_ASSERT(l1 == rclist<int>{1, 2});
  }

  {
    // Splicing to an empty list.
    rclist<int> l1;
    rclist<int> l2{1, 2};
    l1.splice(l1.end(), l2);
    TEST_ASSERT(l1 == rclist<int>{1, 2});
    TEST_ASSERT(l2.empty());
    l1.clear();
    l1.splice(l1.end(), rclist<int>{1, 2});
    TEST_ASSERT(l1 == rclist<int>{1, 2});
  }

  {
    // Empty list shenanigans.
    rclist<int> l1, l2;
    l1.splice(l1.end(), l2);
    TEST_ASSERT(l1.empty());
    TEST_ASSERT(l2.empty());
  }

  {
    rclist<int> l1{1, 2};
    rclist<int> l2{3, 4, 5};
    auto it = std::next(l2.begin());
    l1.splice(l1.begin(), l2, it);
    TEST_ASSERT(l1 == rclist<int>{4, 1, 2});
    TEST_ASSERT(l2 == rclist<int>{3, 5});
  }

  {
    rclist<int> l1{1, 2, 3, 4, 5};
    rclist<int> l2{5, 4, 3, 2, 1};
    l1.splice(std::prev(l1.end(), 2), l2, std::next(l2.begin(), 2), l2.end());
    TEST_ASSERT(l1 == rclist<int>{1, 2, 3, 3, 2, 1, 4, 5});
    TEST_ASSERT(l2 == rclist<int>{5, 4});
  }

  {
    // Multiple splices
    rclist<int> l1;
    rclist<int> l2{1, 2, 3, 4};
    l1.splice(l1.begin(), l2, l2.begin());
    TEST_ASSERT(l1 == rclist<int>{1});
    TEST_ASSERT(l2 == rclist<int>{2, 3, 4});
    l1.splice(l1.begin(), l2, l2.begin());
    TEST_ASSERT(l1 == rclist<int>{2, 1});
    TEST_ASSERT(l2 == rclist<int>{3, 4});
    l1.splice(l1.begin(), l2, l2.begin());
    TEST_ASSERT(l1 == rclist<int>{3, 2, 1});
    TEST_ASSERT(l2 == rclist<int>{4});
  }
}
