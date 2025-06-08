#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/string.h"
#include "nonstd/vector.h"
#include <type_traits>

template <typename T> using rcvector = RCContainer<nonstd::vector, T>;

TEST_CLASS_WITH_FIXTURE(nonstd, vector, constructor, LeakChecker) {
  const string s = "foobar";

  TEST_ASSERT(rcvector<int>{}.empty());
  TEST_ASSERT(rcvector<int>(10) == rcvector<int>{0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  TEST_ASSERT(rcvector<int>(10, 7) ==
              rcvector<int>{7, 7, 7, 7, 7, 7, 7, 7, 7, 7});
  rcvector<char> v(s.begin(), s.end());
  TEST_ASSERT(v == rcvector<char>{'f', 'o', 'o', 'b', 'a', 'r'});
  TEST_ASSERT(rcvector<char>(v) == v);
  TEST_ASSERT(rcvector<int>(rcvector<int>(5)) == rcvector<int>{0, 0, 0, 0, 0});
  TEST_ASSERT(rcvector<char>{'f', 'o', 'o', 'b', 'a', 'r'} == v);

  // Initializer_list is prioritized over constructors 2 and 3 if one
  // or two parameters are provided with curly braces.
  TEST_ASSERT(rcvector<int>{10}.size() == 1);
  TEST_ASSERT(rcvector<int>{10, 7}.size() == 2);
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, assignment, LeakChecker) {
  rcvector<int> v;
  const rcvector<int> v2{1, 2, 3};
  v = v2;
  TEST_ASSERT(v == v2);
  TEST_ASSERT(v == rcvector<int>{1, 2, 3});
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, element_access, LeakChecker) {
  const rcvector<int> v{1, 2, 3};
  TEST_ASSERT(v[0] == 1);
  TEST_ASSERT(v.at(0) == 1);
  TEST_ASSERT(v[1] == 2);
  TEST_ASSERT(v.at(1) == 2);
  TEST_ASSERT(v[2] == 3);
  TEST_ASSERT(v.at(2) == 3);
  TEST_ASSERT(v.front() == 1);
  TEST_ASSERT(v.back() == 3);
  TEST_ASSERT(v.data()[0] == 1);
  TEST_ASSERT(v.data()[1] == 2);
  TEST_ASSERT(v.data()[2] == 3);

  rcvector<int> mutable_v{1, 2};
  mutable_v[0] = 3;
  mutable_v[1] = 4;
  TEST_ASSERT(mutable_v == rcvector<int>{3, 4});
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, iterators, LeakChecker) {
  const rcvector<int> v{1, 2, 3};
  TEST_ASSERT(rcvector<int>(v.begin(), v.end()) == v);
  TEST_ASSERT(rcvector<int>(v.rbegin(), v.rend()) == rcvector<int>{3, 2, 1});

  TEST_ASSERT(*v.begin() == v.front());
  TEST_ASSERT(*std::prev(v.end()) == v.back());
  TEST_ASSERT(*v.rbegin() == v.back());
  TEST_ASSERT(*std::prev(v.rend()) == v.front());
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, capacity, LeakChecker) {
  const rcvector<int> empty;
  TEST_ASSERT(empty.empty());
  TEST_ASSERT(empty.size() == 0);
  TEST_ASSERT(empty.capacity() >= empty.size());

  const rcvector<int> nonempty{1, 2, 3};
  TEST_ASSERT(!nonempty.empty());
  TEST_ASSERT(nonempty.size() == 3);
  TEST_ASSERT(nonempty.capacity() >= nonempty.size());

  // reserve() should not change string contents. The capacity cannot
  // decrease smaller than the string length. Any requests for a
  // smaller capacity are non-binding, i.e. the end capacity is only
  // guaranteed to be >= the requested capacity.
  rcvector<int> v{1};
  TEST_ASSERT(v == rcvector<int>{1});
  TEST_ASSERT(v.capacity() >= v.size());
  size_t requested_capacity = 100;
  v.reserve(requested_capacity);
  TEST_ASSERT(v == rcvector<int>{1});
  TEST_ASSERT(v.capacity() >= requested_capacity);
  requested_capacity = 1;
  v.reserve(requested_capacity);
  TEST_ASSERT(v == rcvector<int>{1});
  TEST_ASSERT(v.capacity() >= requested_capacity);
}

struct TestStructToEmplace {
  TestStructToEmplace(int _a, char _b, nonstd::string _c)
      : a{_a}, b{_b}, c{_c} {}
  int a;
  char b;
  nonstd::string c;
};

TEST_CLASS_WITH_FIXTURE(nonstd, vector, modifiers, LeakChecker) {
  rcvector<int> v{1, 2, 3};
  TEST_ASSERT(!v.empty());
  v.clear();
  TEST_ASSERT(v.empty());

  rcvector<int> v2{1, 2, 3};
  v2.emplace_back(5);
  TEST_ASSERT(v2 == rcvector<int>{1, 2, 3, 5});

  rcvector<TestStructToEmplace> v3;
  v3.emplace_back(1, 'a', "foobar");
  TEST_ASSERT(v3.size() == 1);
  TEST_ASSERT(v3.back().a == 1);
  TEST_ASSERT(v3.back().b == 'a');
  TEST_ASSERT(v3.back().c == "foobar");

  v3.pop_back();
  TEST_ASSERT(v3.empty());

  rcvector<int> v4{1, 2, 3};
  v4.resize(10);
  TEST_ASSERT(v4.size() == 10);
  TEST_ASSERT(v4 == rcvector<int>{1, 2, 3, 0, 0, 0, 0, 0, 0, 0});
  v4.resize(1);
  TEST_ASSERT(v4 == rcvector<int>{1});

  rcvector<int> v5{1, 2, 3};
  rcvector<int> tmp;
  v5.swap(tmp);
  TEST_ASSERT(v5 == rcvector<int>{});
  TEST_ASSERT(tmp == rcvector<int>{1, 2, 3});
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, non_member_functions, LeakChecker) {
  TEST_ASSERT(rcvector<int>{1, 2, 3} == rcvector<int>{1, 2, 3});
  TEST_ASSERT(rcvector<int>{1, 2, 3} != rcvector<int>{3, 2, 1});
}

TEST_CLASS_WITH_FIXTURE(nonstd, vector, realloc, LeakChecker) {
  // Test that we can resize past the current capacity multiple times.
  rcvector<int> v;
  int val = 0;

  // Expected size/capacity at the end of each loop:
  // iter 0: 1/1
  // iter 1: 2/2
  // iter 2: 3/4
  // iter 3: 5/8
  // iter 4: 9/16
  // iter 5: 17/32
  for (size_t i = 0; i < 6; ++i) {
    const size_t cur_cap = v.capacity();
    const size_t cur_size = v.size();
    TEST_ASSERT(cur_cap >= cur_size);
    for (size_t j = 0; j < cur_cap - cur_size; ++j) {
      v.push_back(val++);
      TEST_ASSERT(v.capacity() == cur_cap);
    }
    TEST_ASSERT(v.size() == v.capacity());
    v.push_back(val++);
    TEST_ASSERT(v.capacity() > cur_cap);
  }

  // Ensure that realloc did not mangle the values.
  for (size_t i = 0; i < v.size(); ++i) {
    TEST_ASSERT(v[i] == i);
  }

  // Ensuring this is a reasonable test.
  TEST_ASSERT(val > 10);
  TEST_ASSERT(val == v.size());
}
