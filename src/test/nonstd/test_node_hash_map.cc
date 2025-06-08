#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/node_hash_map.h"
#include "nonstd/string.h"
#include "nonstd/string_view.h"
#include "nonstd/vector.h"

template <typename K, typename V>
using rcnode_hash_map = RCMapContainer<nonstd::node_hash_map, K, V>;

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, constructor, LeakChecker) {
  const rcnode_hash_map<int, int> h1;
  TEST_ASSERT(h1.empty());
  TEST_ASSERT(h1.bucket_count() > 0);

  const rcnode_hash_map<int, int> h2(20);
  TEST_ASSERT(h2.empty());
  TEST_ASSERT(h2.bucket_count() >= 20);

  const vector<std::pair<int, int>> vec{{1, 1}, {2, 2}, {3, 3}};
  const rcnode_hash_map<int, int> h3{vec.begin(), vec.end()};
  TEST_ASSERT(h3.size() == 3);
  TEST_ASSERT(h3.bucket_count() >= 10);

  rcnode_hash_map<int, int> h4{{1, 2}, {3, 4}, {5, 6}, {7, 8}};
  TEST_ASSERT(h4.size() == 4);
  TEST_ASSERT(h4[1] == 2);
  TEST_ASSERT(h4[3] == 4);
  TEST_ASSERT(h4.at(5) == 6);
  TEST_ASSERT(h4.at(7) == 8);

  const rcnode_hash_map<int, int> h5(h3);
  TEST_ASSERT(h5.size() == 3);
  TEST_ASSERT(h5 == h3);

  const rcnode_hash_map<int, int> h6(rcnode_hash_map<int, int>{{1, 2}, {3, 4}});
  TEST_ASSERT(h6.size() == 2);
  TEST_ASSERT(h6 == rcnode_hash_map<int, int>{{1, 2}, {3, 4}});

  // Construct with duplicates
  const rcnode_hash_map<int, int> h7{{1, 1}, {1, 2}, {2, 3}};
  TEST_ASSERT(h7.size() == 2);
  TEST_ASSERT(h7 == rcnode_hash_map<int, int>{{1, 2}, {2, 3}});
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, assignment, LeakChecker) {
  rcnode_hash_map<int, int> h1;
  const rcnode_hash_map<int, int> h2{{0, 1}, {1, 2}, {3, 3}};
  h1 = h2;
  TEST_ASSERT(h1 == h2);
  TEST_ASSERT(h1 == rcnode_hash_map<int, int>{{3, 3}, {1, 2}, {0, 1}});

  h1 = rcnode_hash_map<int, int>{{4, 4}, {5, 5}};
  TEST_ASSERT(h1 == rcnode_hash_map<int, int>{{4, 4}, {5, 5}});
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, iterators, LeakChecker) {
  {
    // non-const
    rcnode_hash_map<int, int> h{{1, 3}, {2, 0}, {4, 3}};
    size_t i = 0;
    for (auto &[k, v] : h) {
      TEST_ASSERT(h[k] == v);
      v = 12345;
      TEST_ASSERT(h[k] == 12345);
      ++i;
    }
    TEST_ASSERT(i == h.size());
  }

  {
    // const
    const rcnode_hash_map<int, int> h{{1, 3}, {2, 0}, {4, 3}};
    size_t i = 0;
    for (const auto &[k, v] : h) {
      TEST_ASSERT(h.contains(k));
      TEST_ASSERT(h.at(k) == v);
      ++i;
    }
    TEST_ASSERT(i == h.size());
  }

  {
    // erase while iterating
    rcnode_hash_map<int, int> h{{1, 3}, {2, 0}, {4, 3}};
    const size_t orig_size = h.size();
    size_t i = 0;
    for (auto it = h.begin(); it != h.end();) {
      it = h.erase(it);
      ++i;
    }
    TEST_ASSERT(i == orig_size);
    TEST_ASSERT(h.empty());
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, capacity, LeakChecker) {
  TEST_ASSERT(rcnode_hash_map<int, int>{}.empty());
  TEST_ASSERT(rcnode_hash_map<int, int>{}.size() == 0);

  TEST_ASSERT(!rcnode_hash_map<int, int>{{1, 1}}.empty());
  TEST_ASSERT(rcnode_hash_map<int, int>{{1, 1}}.size() == 1);
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, modifiers, LeakChecker) {
  {
    // clear
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(!h.empty());
    h.clear();
    TEST_ASSERT(h.empty());
  }

  {
    // insert/emplace
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(!h.contains(5));
    auto [it, inserted] = h.insert({5, 6});
    TEST_ASSERT(inserted);
    TEST_ASSERT(it->first == 5);
    TEST_ASSERT(it->second == 6);
    TEST_ASSERT(h[5] == 6);

    TEST_ASSERT(h[3] == 4);
    auto it2 = h.find(3);
    std::tie(it, inserted) = h.insert({3, 7});
    TEST_ASSERT(!inserted);
    TEST_ASSERT(it->first == 3);
    TEST_ASSERT(it->second == 7);
    TEST_ASSERT(it2 == it);
    TEST_ASSERT(h[3] == 7);

    TEST_ASSERT(!h.contains(7));
    std::tie(it, inserted) = h.emplace(7, 8);
    TEST_ASSERT(inserted);
    TEST_ASSERT(it->first == 7);
    TEST_ASSERT(it->second == 8);
    TEST_ASSERT(h[7] == 8);
  }

  {
    // try_emplace
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(!h.contains(5));
    auto [it, inserted] = h.try_emplace(5, 6);
    TEST_ASSERT(inserted);
    TEST_ASSERT(it->first == 5);
    TEST_ASSERT(it->second == 6);
    TEST_ASSERT(h[5] == 6);

    TEST_ASSERT(h[3] == 4);
    auto it2 = h.find(3);
    std::tie(it, inserted) = h.try_emplace(3, 7);
    TEST_ASSERT(!inserted);
    TEST_ASSERT(it->first == 3);
    TEST_ASSERT(it->second == 4);
    TEST_ASSERT(it2 == it);
    TEST_ASSERT(h[3] == 4);
  }

  {
    // erase
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    const auto it = h.find(3);
    TEST_ASSERT(it != h.end());
    TEST_ASSERT(h.find(1) != h.end());
    TEST_ASSERT(h.size() == 2);
    const auto next = std::next(it);
    const auto it2 = h.erase(it);
    TEST_ASSERT(next == it2);
    TEST_ASSERT(h.size() == 1);
    TEST_ASSERT(h.find(3) == h.end());
    TEST_ASSERT(h.find(1) != h.end());

    h.emplace(3, 5);
    TEST_ASSERT(h.size() == 2);
    TEST_ASSERT(h.find(3) != h.end());
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, lookup, LeakChecker) {
  {
    // const at
    const rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(h.at(1) == 2);
    TEST_ASSERT(h.at(3) == 4);
  }

  {
    // non-const at/operator[]
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(h.at(1) == 2);
    TEST_ASSERT(h.at(3) == 4);
    TEST_ASSERT(h[1] == 2);
    TEST_ASSERT(h[3] == 4);

    // operator[] on a non-existent key acts like try_emplace(k),
    // i.e., it'll default-construct the value.
    TEST_ASSERT(h.size() == 2);
    TEST_ASSERT(h[5] == int{});
    TEST_ASSERT(h.size() == 3);
  }

  {
    // const find/contains
    const rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(h.contains(1));
    TEST_ASSERT(h.find(1) != h.end());

    TEST_ASSERT(!h.contains(5));
    TEST_ASSERT(h.find(5) == h.end());
  }

  {
    // non-const find
    rcnode_hash_map<int, int> h{{1, 2}, {3, 4}};
    TEST_ASSERT(h.contains(1));
    auto it = h.find(1);
    TEST_ASSERT(it != h.end());
    it->second = 54321;

    TEST_ASSERT(h == rcnode_hash_map<int, int>{{3, 4}, {1, 54321}});
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, hash_policy, LeakChecker) {
  // These tests assume a maximum load factor of 1.
  {
    // reserve: technically just calls rehash but the intention is
    // different, especially when the load factor is not 1. We test
    // that the bucket count increases but the hashtable contents
    // remain unchanged.
    rcnode_hash_map<int, int> h1{{1, 2}, {3, 4}};
    const auto h2 = h1;
    TEST_ASSERT(h1.bucket_count() < 100);
    h1.reserve(200);
    TEST_ASSERT(h1.bucket_count() >= 100);
    TEST_ASSERT(h1 == h2);
    TEST_ASSERT(h1.bucket_count() != h2.bucket_count());
  }

  {
    // rehash: ensure that contents get resized automatically.
    rcnode_hash_map<int, int> h;
    for (size_t i = 0, rehashes = 0, bucket_count = h.bucket_count();
         rehashes < 4; ++i) {
      h[i] = /* some arbitrary function */ 1 + 2 * i;
      if (h.bucket_count() > bucket_count) {
        bucket_count = h.bucket_count();
        ++rehashes;
      }
    }

    for (auto [k, v] : h) {
      TEST_ASSERT(v == 1 + 2 * k);
    }
    // Make sure this test is reasonable.
    TEST_ASSERT(h.size() > 100);
  }
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, non_member_functions, LeakChecker) {
  TEST_ASSERT(rcnode_hash_map<int, int>{} == rcnode_hash_map<int, int>{});
  TEST_ASSERT(rcnode_hash_map<int, int>{} != rcnode_hash_map<int, int>{{1, 2}});
  TEST_ASSERT(rcnode_hash_map<int, int>{{2, 1}} != rcnode_hash_map<int, int>{});
  TEST_ASSERT(rcnode_hash_map<int, int>{{1, 2}, {3, 4}} ==
              rcnode_hash_map<int, int>{{1, 2}, {3, 4}});
  TEST_ASSERT(rcnode_hash_map<int, int>{{1, 2}, {3, 4}} !=
              rcnode_hash_map<int, int>{{1, 2}, {3, 5}});

  // Comparison (naturally) shouldn't include duplicates.
  TEST_ASSERT(rcnode_hash_map<int, int>{{1, 2}, {1, 4}} ==
              rcnode_hash_map<int, int>{{1, 4}});

  // Insertion order doesn't matter.
  TEST_ASSERT(rcnode_hash_map<int, int>{{1, 2}, {3, 4}} ==
              rcnode_hash_map<int, int>{{3, 4}, {1, 2}});
}

TEST_CLASS_WITH_FIXTURE(nonstd, node_hash_map, heterogeneous, LeakChecker) {
  // Heterogeneous lookup (a.k.a. "transparent hash/equality policies"
  // according to the cpp spec) involves lookups using a different
  // type without constructing the key type.
  rcnode_hash_map<nonstd::string, int> h{{"hello", 1}, {"world", 2}};
  TEST_ASSERT(h[string{"hello"}] == 1);
  TEST_ASSERT(h[string{"world"}] == 2);
  // Shouldn't have any more string constructors after this. This can
  // be easily checked by printing from the string constructor.
  TEST_ASSERT(h["hello"_sv] == 1);
  TEST_ASSERT(h["world"_sv] == 2);
  TEST_ASSERT(h[/*(const char[6])*/ "hello"] == 1);
  TEST_ASSERT(h[/*(const char[6])*/ "world"] == 2);
  TEST_ASSERT(h[(const char *)"hello"] == 1);
  TEST_ASSERT(h[(const char *)"world"] == 2);
}
