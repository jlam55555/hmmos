#pragma once

/// \file
/// \brief Simple implementation of C++20/23-ish string_view. Features
/// are implemented as needed.

#include "nonstd/libc.h"
#include "perf.h"
#include <algorithm>
#include <cstddef>

namespace nonstd {

/// \brief Nonstd version of std::string_view.
///
class string_view {
public:
  constexpr string_view(const char *s) : _data(s), _len(strlen(s)) {}
  constexpr string_view(const char *s, size_t len) : _data(s), _len(len) {}

  constexpr const char *data() const { return _data; }
  constexpr size_t length() const { return _len; }
  constexpr size_t size() const { return _len; }
  constexpr bool empty() const { return _len == 0; };

  constexpr bool operator==(const string_view &rhs) const {
    return _len == rhs._len && !strncmp(_data, rhs._data, _len);
  }

  constexpr auto operator<=>(const string_view &rhs) const {
    const size_t cmp_len = std::min(_len, rhs._len);
    const auto res = nonstd::strncmp(_data, rhs._data, cmp_len);
    if (res != 0) {
      return res <=> 0;
    }
    return _len <=> rhs._len;
  }

  constexpr const char operator[](size_t pos) const { return _data[pos]; }

  static constexpr size_t npos = -1;

  constexpr size_t find(string_view needle) const {
    if (size() < needle.size()) {
      return npos;
    }
    for (size_t i = 0; i <= size() - needle.size(); ++i) {
      if (needle == string_view{data() + i, needle.size()}) {
        return i;
      }
    }
    return npos;
  }
  constexpr size_t find(char needle) const {
    // memchr
    for (size_t i = 0; i < size(); ++i) {
      if (data()[i] == needle) {
        return i;
      }
    }
    return npos;
  }

  /// \brief Returns a substring of the original string_view.
  ///
  /// This is different behavior than the standard, since this doesn't
  /// throw if out of bounds. Instead, it silently crops to the
  /// bounds. Clearly will also have unwanted behavior if `start` is a
  /// negative signed int.
  constexpr string_view substr(int32_t start, size_t count = npos) const {
    if (unlikely(start < 0)) {
      start = 0;
    }
    if (unlikely(start > size())) {
      start = size();
    }
    if (count == npos || unlikely(start + count > size())) {
      count = size() - start;
    }
    return {data() + start, count};
  }
  constexpr string_view &operator=(const string_view &rhs) {
    _len = rhs.size();
    _data = rhs.data();
    return *this;
  }
  constexpr bool starts_with(string_view needle) {
    return substr(0, needle.size()) == needle;
  }
  constexpr bool starts_with(char needle) {
    return size() && data()[0] == needle;
  }
  constexpr bool ends_with(string_view needle) {
    return substr(size() - needle.size()) == needle;
  }
  constexpr bool ends_with(char needle) {
    return size() && data()[size() - 1] == needle;
  }

  // C++23 :)
  constexpr bool contains(string_view needle) { return find(needle) != npos; }

private:
  const char *_data;
  size_t _len;
};

} // namespace nonstd

constexpr nonstd::string_view operator""_sv(const char *s, size_t len) {
  return {s, len};
}
