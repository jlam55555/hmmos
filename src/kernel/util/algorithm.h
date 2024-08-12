#pragma once

/// \file
/// \brief Useful reusable algorithms and bit-twiddling. Not much
/// here, will add things as needed.
///
/// This is not intended to be for algorithms from the <algorithm>
/// header -- if we need custom implementations they should go in the
/// nonstd/ directory.

#include <stdint.h>

namespace util::algorithm {

constexpr bool pow2(uint64_t n) { return !!n & !(n & (n - 1)); }

template <uint64_t divisor>
constexpr uint64_t ceil_pow2(uint64_t n) requires(pow2(divisor)) {
  return (n - 1 + divisor) & ~(divisor - 1);
}

template <uint64_t divisor>
constexpr uint64_t floor_pow2(uint64_t n) requires(pow2(divisor)) {
  return n & ~(divisor - 1);
}

template <uint64_t divisor>
constexpr bool aligned_pow2(uint64_t n) requires(pow2(divisor)) {
  return !(n & (divisor - 1));
}

/// \brief Test if two ranges overlap. Ranges are specified using
/// (start, end).
template <typename T>
constexpr bool range_overlaps(T base1, T end1, T base2, T end2,
                              bool inclusive = true) {
  if (inclusive) {
    return base1 <= end2 && base2 <= end1;
  } else {
    return base1 < end2 && base2 < end1;
  }
}

/// \brief Test if two ranges overlap. Ranges are specified using
/// (start, len).
template <typename T>
constexpr bool range_overlaps2(T base1, T len1, T base2, T len2,
                               bool inclusive = true) {
  return range_overlaps(base1, base1 + len1, base2, base2 + len2, inclusive);
}

/// \brief Test if the first range subsumes the second range.
///
template <typename T>
constexpr bool range_subsumes(T base1, T end1, T base2, T end2,
                              bool inclusive = true) {
  if (inclusive) {
    return base1 <= base2 && end1 >= end2;
  } else {
    return base1 < base2 && end1 > end2;
  }
}

template <typename T>
constexpr bool range_subsumes2(T base1, T len1, T base2, T len2,
                               bool inclusive = true) {
  return range_subsumes(base1, base1 + len1, base2, base2 + len2, inclusive);
}

} // namespace util::algorithm
