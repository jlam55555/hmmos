#pragma once

/// \file polyfill.h
/// \brief Future standard language features not supported by the
/// current compiler (C++20), that don't fit better in other nonstd
/// headers.

#include <type_traits>

namespace nonstd {

// <utility>, cpp23
template <typename Enum>
requires std::is_enum_v<Enum>
auto to_underlying(Enum e) {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

} // namespace nonstd
