#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nonstd {

namespace mem {
static inline uint64_t alloc_count = 0;
static inline uint64_t dealloc_count = 0;
} // namespace mem

/// Copied from https://en.cppreference.com/w/cpp/named_req/Allocator
template <class T> struct Mallocator {
  typedef T value_type;

  Mallocator() = default;

  template <class U> constexpr Mallocator(const Mallocator<U> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
      assert(((void)"ALLOC_TOO_LARGE", false));
    }

    if (auto p = static_cast<T *>(::operator new(n * sizeof(T)))) {
      ++mem::alloc_count;
      return p;
    }

    assert(((void)"OOM", false));
  }

  void deallocate(T *p, std::size_t n) noexcept {
    ++mem::dealloc_count;
    ::operator delete(p);
  }
};

} // namespace nonstd
