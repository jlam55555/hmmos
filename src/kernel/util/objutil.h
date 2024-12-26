#pragma once

/// \file objutil.h
/// \brief Utility functions/macros for C++ objects.

#include "util/bw.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

/// If an object manages some resource, and copying doesn't produce an
/// equivalent result.
#define NON_COPYABLE(Class)                                                    \
  Class(Class &) = delete;                                                     \
  Class &operator=(Class &) = delete;

/// If an object needs to have a stable reference, e.g., if a pointer
/// is stored in some container. E.g., any object containing an
/// intrusive list should be marked NON_MOVABLE.
#define NON_MOVABLE(Class)                                                     \
  NON_COPYABLE(Class)                                                          \
  Class(Class &&) = delete;                                                    \
  Class &operator=(Class &&) = delete;

namespace objutil {

/// Allow copying a volatile struct of standard integer size into a
/// non-volatile struct, or vice-versa.
///
/// This is useful to copy organized data or bitfields from a volatile
/// struct without using a union/conversion to int type first.
///
/// ```
/// struct MyVolatileStructImpl {
///   uint8_t foo;
///   uint8_t bar;
///   struct {
///     uint8_t flag1 : 1;
///     uint8_t flag2 : 1;
///     uint8_t flag3 : 1;
///     uint8_t flag4 : 1;
///     uint8_t flag5 : 4;
///   } bitfield;
///   uint8_t baz;
/// };
/// using MyVolatileStruct = volatile MyVolatileStruct;
///
/// MyVolatileStruct mvs = ...;
///
/// // Assignment from volatile to non-volatile struct not allowed
/// // auto bitfieldCopy = mvs.bitfield;
///
/// // We can cast away cv-qualifiers, which will compile but is not
/// // safe because it ends up being a regular struct assignment which
/// // is not generally safe with volatile instances.
/// // auto bitfield = const_cast<decltype(mvs.bitfield)&>(mvs.bitfield);
///
/// // Assigning a volatile int to a non-volatile int is safe, and
/// // vice versa.
/// uint8_t tmp = reinterpret_cast<volatile uint8_t&>(mvs.bitfield);
/// auto bitfield = reinterpret_cast<decltype(mvs.bitfield)&>(tmp);
/// ```
///
template <bw::BitfieldLike A> A copy_from_volatile(const volatile A &src) {
  auto tmp = bw::id(src);
  return reinterpret_cast<A &>(tmp);
}

/// \see copy_from_volatile
template <bw::BitfieldLike A>
void copy_to_volatile(volatile A &dst, const A &src) {
  auto tmp = bw::id(src);
  reinterpret_cast<volatile decltype(tmp) &>(dst) = tmp;
}

} // namespace objutil
