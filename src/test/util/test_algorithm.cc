#include "../test.h"
#include "util/algorithm.h"

TEST(util::algorithm, pow2) {
  TEST_ASSERT(!pow2(0));

  for (unsigned i = 0; i < 64; ++i) {
    uint64_t n = 1ULL << i;
    TEST_ASSERT(pow2(n));
    if (n > 2) {
      TEST_ASSERT(!pow2(n - 1));
      TEST_ASSERT(!pow2(n + 1));
    }
  }
}

TEST(util::algorithm, ceil_pow2) {
  TEST_ASSERT(ceil_pow2<4096>(0) == 0);
  TEST_ASSERT(ceil_pow2<4096>(1) == 4096);
  TEST_ASSERT(ceil_pow2<4096>(4095) == 4096);
  TEST_ASSERT(ceil_pow2<4096>(4096) == 4096);
  TEST_ASSERT(ceil_pow2<4096>(4097) == 8192);
}

TEST(util::algorithm, floor_pow2) {
  TEST_ASSERT(floor_pow2<4096>(0) == 0);
  TEST_ASSERT(floor_pow2<4096>(1) == 0);
  TEST_ASSERT(floor_pow2<4096>(4095) == 0);
  TEST_ASSERT(floor_pow2<4096>(4096) == 4096);
  TEST_ASSERT(floor_pow2<4096>(4097) == 4096);
}

TEST(util::algorithm, aligned_pow2) {
  TEST_ASSERT(aligned_pow2<4096>(0));
  TEST_ASSERT(!aligned_pow2<4096>(1));
  TEST_ASSERT(!aligned_pow2<4096>(4095));
  TEST_ASSERT(aligned_pow2<4096>(4096));
  TEST_ASSERT(!aligned_pow2<4096>(4097));
}

TEST(util::algorithm, range_overlaps_inclusive) {
  // One completely overlaps the other, plus some on both ends.
  TEST_ASSERT(range_overlaps(0, 3, 1, 2));
  TEST_ASSERT(range_overlaps(1, 2, 0, 3));

  // One completely overlaps the other, plus some on one end.
  TEST_ASSERT(range_overlaps(0, 1, 0, 2));
  TEST_ASSERT(range_overlaps(0, 2, 0, 1));

  // Inclusive overlap at the edge.
  TEST_ASSERT(range_overlaps(0, 1, 1, 2));
  TEST_ASSERT(range_overlaps(1, 2, 0, 1));

  // No overlap.
  TEST_ASSERT(!range_overlaps(0, 1, 2, 3));
  TEST_ASSERT(!range_overlaps(2, 3, 0, 1));
}

TEST(util::algorithm, range_overlaps_exclusive) {
  TEST_ASSERT(range_overlaps(0, 3, 1, 2, false));
  TEST_ASSERT(range_overlaps(1, 2, 0, 3, false));

  TEST_ASSERT(range_overlaps(0, 1, 0, 2, false));
  TEST_ASSERT(range_overlaps(0, 2, 0, 1, false));

  TEST_ASSERT(!range_overlaps(0, 1, 1, 2, false));
  TEST_ASSERT(!range_overlaps(1, 2, 0, 1, false));

  TEST_ASSERT(!range_overlaps(0, 1, 2, 3, false));
  TEST_ASSERT(!range_overlaps(2, 3, 0, 1, false));
}

TEST(util::algorithm, range_subsumes_inclusive) {
  TEST_ASSERT(range_subsumes(0, 3, 1, 2));
  TEST_ASSERT(!range_subsumes(1, 2, 0, 3));

  TEST_ASSERT(!range_subsumes(0, 1, 0, 2));
  TEST_ASSERT(range_subsumes(0, 2, 0, 1));

  TEST_ASSERT(!range_subsumes(0, 1, 1, 2));
  TEST_ASSERT(!range_subsumes(1, 2, 0, 1));

  TEST_ASSERT(!range_subsumes(0, 1, 2, 3));
  TEST_ASSERT(!range_subsumes(2, 3, 0, 1));
}

TEST(util::algorithm, range_subsumes_exclusive) {
  TEST_ASSERT(range_subsumes(0, 3, 1, 2, false));
  TEST_ASSERT(!range_subsumes(1, 2, 0, 3, false));

  TEST_ASSERT(!range_subsumes(0, 1, 0, 2, false));
  TEST_ASSERT(!range_subsumes(0, 2, 0, 1, false));

  TEST_ASSERT(!range_subsumes(0, 1, 1, 2, false));
  TEST_ASSERT(!range_subsumes(1, 2, 0, 1, false));

  TEST_ASSERT(!range_subsumes(0, 1, 2, 3, false));
  TEST_ASSERT(!range_subsumes(2, 3, 0, 1, false));
}
