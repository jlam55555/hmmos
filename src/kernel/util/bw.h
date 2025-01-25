#pragma once

/// \file bw.h
/// \brief Bitwise operations, especially on structs
///
/// It would be nice if these could be constexpr, but alas we are
/// doing some sketchy stuff here (reinterpret_cast<> is not allowed
/// in constexpr contexts and probably for good reason). However, this
/// should be expected to work when structs are little-endian and
/// properly packed.
///
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace bw {

namespace detail {
/// Helpers for struct bitwise-AND below.
template <size_t> struct IntTypeImpl { using T = void; };
template <> struct IntTypeImpl<1> { using T = uint8_t; };
template <> struct IntTypeImpl<2> { using T = uint16_t; };
template <> struct IntTypeImpl<4> { using T = uint32_t; };
template <> struct IntTypeImpl<8> { using T = uint64_t; };
template <size_t size> using IntType = typename IntTypeImpl<size>::T;
} // namespace detail

/// \brief A struct that can be treated either as a bitfield or an integer.
///
template <typename B>
concept BitfieldLike =
    std::is_trivially_copyable_v<std::remove_cvref_t<B>> && !std::is_void_v<B>;

// Integer types should of course satisfy BitfieldLike
static_assert(BitfieldLike<uint8_t> && BitfieldLike<uint16_t> &&
              BitfieldLike<uint32_t> && BitfieldLike<uint64_t>);

/// Turns a BitfieldLike object into its integer representation
///
/// For integer types, this is a no-op (identity). For structs, it is
/// a reinterpret_cast<> into the integer type.
template <BitfieldLike B> detail::IntType<sizeof(B)> id(const B &b) {
  return reinterpret_cast<detail::IntType<sizeof(B)> &&>(
      const_cast<std::remove_cv_t<B> &&>(b));
}

/// Returns the inverse of the BitfieldLike object.
///
/// If _RetT is specified, it will cast the return type to _RetT -- by
/// default it will return the integer representation.
template <typename _RetT = void, BitfieldLike B,
          BitfieldLike RetT = std::conditional_t<
              std::is_void_v<_RetT>, detail::IntType<sizeof(B)>, _RetT>>
RetT not_(const B &b) {
  auto tmp = ~id(b);
  return reinterpret_cast<RetT &>(tmp);
}

namespace detail {
template <typename _RetT = void, BitfieldLike B1, BitfieldLike B2,
          BitfieldLike RetT = std::conditional_t<
              std::is_void_v<_RetT>, detail::IntType<sizeof(B1)>, _RetT>>
requires(sizeof(B1) == sizeof(B2)) RetT and_(B1 &&b1, B2 &&b2) {
  auto tmp = id(b1) & id(b2);
  return reinterpret_cast<RetT &>(tmp);
}

template <typename _RetT = void, BitfieldLike B1, BitfieldLike B2,
          BitfieldLike RetT = std::conditional_t<
              std::is_void_v<_RetT>, detail::IntType<sizeof(B1)>, _RetT>>
requires(sizeof(B1) == sizeof(B2)) RetT or_(B1 &&b1, B2 &&b2) {
  auto tmp = id(b1) | id(b2);
  return reinterpret_cast<RetT &>(tmp);
}

} // namespace detail

/// Returns the bitwise-AND of the BitfieldLike objects.
///
/// If _RetT is specified, it will cast the return type to _RetT -- by
/// default it will return the integer representation.
///
/// The motivation behind this was to provide a more structured way to
/// check/set flags in a bitfield. Rather than redefining each flag as
/// a constant that exists independently of the struct definition:
///
/// ```
/// #define FLAG_1 0x01
/// #define FLAG_2 0x02
/// #define FLAG_3 0x04
/// #define FLAG_4 = 0x08
/// #define FIELD_5 = 0xF0
/// ...
///
/// uint8_t my_bitfield = ...;
/// bool check_some_flags = my_bitfield & (FLAG_1 | FLAG_3);
/// uint8_t field_5 = (my_bitfield & FIELD_5) >> 4;
/// ```
///
/// ... with this interface you can bitwise-AND structs directly:
///
/// ```
/// struct MyBitfield : ImplicitBitfield {
///   uint8_t flag_1 : 1;
///   uint8_t flag_2 : 1;
///   uint8_t flag_3 : 1;
///   uint8_t flag_4 : 1;
///   uint8_t field_5 : 4;
/// };
///
/// MyBitfield my_bitfield = ...;
/// bool check_some_flags =
///     bw::and_(my_bitfield, MyBitfield{.flag_1=1, .flag_3=1});
/// uint8_t field_5 = bw::and_(my_bitfield, MyBitfield{.field_5}) >> 4;
/// ```
///
/// Whether this actually helps with maintainability is something to
/// find out, but to me this seems readable. I think the "old" method
/// is fine if the struct is defined with opaque integer bitfields,
/// but this new method makes sense if the struct is already defined
/// as a bitfield and you don't want to duplicate the field
/// definitions as constants.
///
template <typename RetT = void, BitfieldLike B, BitfieldLike... Bs>
requires(sizeof...(Bs) > 0) auto and_(B &&b, Bs &&...bs) {
  if constexpr (sizeof...(Bs) == 1) {
    return detail::and_<RetT>(b, bs...);
  } else {
    return detail::and_<RetT>(b, and_<RetT>(bs...));
  }
}

/// Returns the bitwise-OR of the BitfieldLike objects.
///
/// If _RetT is specified, it will cast the return type to _RetT -- by
/// default it will return the integer representation.
template <typename RetT = void, BitfieldLike B, BitfieldLike... Bs>
requires(sizeof...(Bs) > 0) auto or_(B &&b, Bs &&...bs) {
  if constexpr (sizeof...(Bs) == 1) {
    return detail::or_<RetT>(b, bs...);
  } else {
    return detail::or_<RetT>(b, or_<RetT>(bs...));
  }
}

} // namespace bw
