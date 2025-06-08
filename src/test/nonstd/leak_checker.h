#pragma once

/// \file
/// \brief Leak checking utils for nonstd container types
///
/// This file contains two leak checks:
///
/// 1. Net (de)allocations == 0: Over the course of a test, all
///    allocations (via the Allocator) should be met with a
///    deallocation. This doesn't actually check that allocations
///    match with a deallocation, but the net count should suffice.
///    This is checked by using the LeakChecker test fixture.
/// 2. Net (de)constructions == 0: Even if the container's use of
///    allocations doesn't leak, it's possible that elements in the
///    container are not properly constructed/destructed. A prime
///    example is nonstd::vector, which allocates a backing store and
///    then calls (de)constructors via placement new and calling the
///    destructor explicitly. This is checked by using the LeakChecker
///    test fixture and using the RCContainer wrapper.
///

#include "../test.h"
#include "nonstd/allocator.h"
#include "nonstd/hash_bytes.h"
#include <concepts>
#include <utility>

/// \brief Test fixture that must be used for the net allocation and
/// net construction checks.
class LeakChecker : public test::TestFixture {
public:
  static void construct() { ++construct_count; }
  static void destruct() { ++destruct_count; }

private:
  void setup() final {
    construct_count = 0;
    destruct_count = 0;
    start_alloc_count = nonstd::mem::alloc_count;
    start_dealloc_count = nonstd::mem::dealloc_count;
  }
  void destroy(bool &_test_passed) final {
    // Reset so the following tests pass.
    const uint64_t net_allocs = nonstd::mem::alloc_count - start_alloc_count;
    const uint64_t net_deallocs =
        nonstd::mem::dealloc_count - start_dealloc_count;
#ifdef DEBUG
    if (construct_count != destruct_count || net_allocs != net_deallocs) {
      nonstd::printf("leak_checker failed:\r\n"
                     "\tconstruct=%llu\r\n"
                     "\tdestruct=%llu\r\n"
                     "\tnet_allocs=%llu\r\n"
                     "\tnet_deallocs=%llu\r\n",
                     construct_count, destruct_count, net_allocs, net_deallocs);
    }
#endif
    TEST_ASSERT(construct_count == destruct_count);
    TEST_ASSERT(net_allocs == net_deallocs);
  }

  static uint64_t construct_count;
  static uint64_t destruct_count;
  uint64_t start_alloc_count = 0;
  uint64_t start_dealloc_count = 0;
};

namespace {

/// \brief Wrapper around an element in RCWrapper. It increments the
/// (de)constructor counts on (de)construction.
///
/// We need separate templates for scalar vs. non-scalar types, since
/// you can't subclass from a scalar type. However, we need
/// subclassing for non-scalar types to inherit public members.
template <typename T> class RCWrapper;
template <std::integral T> class RCWrapper<T> {
public:
  // Need to override the defaulted special member functions, since
  // they don't seem to get suppressed by the templated constructor?
  RCWrapper() : val() { LeakChecker::construct(); }
  RCWrapper(const RCWrapper &v) : val(v) { LeakChecker::construct(); }
  RCWrapper(RCWrapper &&v) : val(v) { LeakChecker::construct(); }
  template <typename... Args>
  RCWrapper(Args &&...args) : val(std::forward<Args>(args)...) {
    LeakChecker::construct();
  }
  ~RCWrapper() { LeakChecker::destruct(); }
  RCWrapper &operator=(RCWrapper &) = default;
  RCWrapper &operator=(RCWrapper &&) = default;
  operator T() const { return val; }

private:
  T val;
};
template <typename T> class RCWrapper : public T {
public:
  RCWrapper() : T() { LeakChecker::construct(); }
  RCWrapper(const RCWrapper &val) : T(val) { LeakChecker::construct(); }
  RCWrapper(RCWrapper &&val) : T(std::move(val)) { LeakChecker::construct(); }
  template <typename... Args>
  RCWrapper(Args &&...args) : T(std::forward<Args>(args)...) {
    LeakChecker::construct();
  }
  ~RCWrapper() { LeakChecker::destruct(); }
  RCWrapper &operator=(RCWrapper &other) = default;
  RCWrapper &operator=(RCWrapper &&) = default;
  operator T() const { return static_cast<T &>(*this); }
};

/// Wrapper around a container that keeps a counter of element
/// (de)constructions.
///
/// \see \ref RCContainer and \ref RCMapContainer for more convenient
/// forms.
template <typename Container> class _RCContainer : public Container {
public:
  using Container::Container;
  _RCContainer(const Container &c) { static_cast<Container &>(*this) = c; }
  operator Container &() const { return static_cast<Container &>(*this); }
};

} // namespace

namespace std {
// std::common_type_t (used in heterogenous lookups) cannot infer the
// common type of RCWrapper<T> and T even though there is an implicit
// conversion defined, probably because it's a templated type.
template <typename T, typename U> struct common_type<U, RCWrapper<T>> {
  using type = std::common_type_t<T, U>;
};
template <typename T, typename U> struct common_type<RCWrapper<T>, U> {
  using type = std::common_type_t<T, U>;
};
} // namespace std

namespace nonstd {
template <typename T> struct hash<RCWrapper<T>> {
  size_t operator()(const RCWrapper<T> &key) { return hash<T>{}(key); }
};
} // namespace nonstd

template <template <typename...> typename Container, typename T>
using RCContainer = _RCContainer<Container<RCWrapper<T>>>;

template <template <typename...> typename Container, typename K, typename V>
using RCMapContainer = _RCContainer<Container<RCWrapper<K>, RCWrapper<V>>>;
