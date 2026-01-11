#include "../nonstd/leak_checker.h"
#include "../test.h"
#include "util/lru.h"

TEST_CLASS_WITH_FIXTURE(util, LRUCache, basic, LeakChecker) {
  unsigned populate_cache_calls = 0;
  unsigned evictions = 0;
  LRUCache<int, int> cache{[&populate_cache_calls](int v) {
                             ++populate_cache_calls;
                             return v + 2;
                           },
                           [&evictions](int, int) { ++evictions; }};

  unsigned expected_cache_size = 0;
  unsigned expected_populate_cache_calls = 0;
  unsigned expected_evictions = 0;

  // Insert items, see that cache gets populated transparently.
  for (int i : {1, 2, 3, 4, 5}) {
    TEST_ASSERT(cache.get(i) == i + 2);
    TEST_ASSERT(cache.size() == ++expected_cache_size);
    TEST_ASSERT(populate_cache_calls == ++expected_populate_cache_calls);
    TEST_ASSERT(evictions == expected_evictions);
  }

  // Cache should not grow if we access a resident item.
  TEST_ASSERT(cache.get(3) == 5);
  TEST_ASSERT(cache.size() == expected_cache_size);
  TEST_ASSERT(populate_cache_calls == expected_populate_cache_calls);
  TEST_ASSERT(evictions == expected_evictions);

  // Eviction should happen in LRU order.
  // Evict two oldest items.
  TEST_ASSERT(cache.evict(3) == 2);
  TEST_ASSERT(evictions == 2);
  expected_cache_size = 3;
  expected_evictions = 2;

  // Items still in the cache. Note that calling get() (or more
  // exactly, the order in which we release the @a Lease) affects the
  // LRU cache. Now 4 is the oldest item, followed by 5, 3.
  for (int i : {4, 5, 3}) {
    TEST_ASSERT(cache.get(i) == i + 2);
    TEST_ASSERT(cache.size() == expected_cache_size);
    TEST_ASSERT(populate_cache_calls == expected_populate_cache_calls);
    TEST_ASSERT(evictions == expected_evictions);
  }
  // Item not in the cache
  for (int i : {2, 1}) {
    TEST_ASSERT(cache.get(i) == i + 2);
    TEST_ASSERT(cache.size() == ++expected_cache_size);
    TEST_ASSERT(populate_cache_calls == ++expected_populate_cache_calls);
    TEST_ASSERT(evictions == expected_evictions);
  }

  // Evict oldest items in order.
  for (int i : {4, 5, 3, 2, 1}) {
    TEST_ASSERT(cache.evict(--expected_cache_size) == 1);
    TEST_ASSERT(cache.size() == expected_cache_size);
    TEST_ASSERT(cache.get(i, /*insert_if_missing=*/false).empty());
    TEST_ASSERT(evictions == ++expected_evictions);
  }
}

TEST_CLASS_WITH_FIXTURE(util, LRUCache, leases, LeakChecker) {
  LRUCache<int, int> cache{[](int v) { return v; }, [](int, int) {}};

  cache.get(1);
  auto l1 = cache.get(2);
  cache.get(3);
  auto l2 = cache.get(4);
  cache.get(5);

  TEST_ASSERT(cache.size() == 5);
  TEST_ASSERT(cache.evict(0) == 3);
  TEST_ASSERT(cache.get(2, /*insert_if_missing=*/false) == 2);
  TEST_ASSERT(cache.get(4, /*insert_if_missing=*/false) == 4);

  l1.~Lease();
  TEST_ASSERT(cache.evict(0) == 1);
  TEST_ASSERT(cache.size() == 1);
  TEST_ASSERT(cache.get(2, /*insert_if_missing=*/false).empty());
  TEST_ASSERT(cache.get(4, /*insert_if_missing=*/false) == 4);
}

TEST_CLASS_WITH_FIXTURE(util, LRUCache, bypass_lru, LeakChecker) {
  LRUCache<int, int> cache{[](int v) { return v; }, [](int, int) {}};

  for (int i : {1, 2, 3, 4, 5}) {
    cache.get(i);
  }
  TEST_ASSERT(cache.size() == 5);

  auto lease = cache.get(3, /*insert_if_missing=*/false);
  TEST_ASSERT(!lease.empty());

  // This is the most recently used, so it wouldn't be evicted
  // normally. \a evict_one() bypasses the LRU mechanism
  cache.evict_one(std::move(lease));

  TEST_ASSERT(cache.size() == 4);
  TEST_ASSERT(!cache.get(1, /*insert_if_missing=*/false).empty());
  TEST_ASSERT(!cache.get(2, /*insert_if_missing=*/false).empty());
  TEST_ASSERT(cache.get(3, /*insert_if_missing=*/false).empty());
  TEST_ASSERT(!cache.get(4, /*insert_if_missing=*/false).empty());
  TEST_ASSERT(!cache.get(5, /*insert_if_missing=*/false).empty());
}
