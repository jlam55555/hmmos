#pragma once

/// \file
/// \brief Simple implementation of C++20/23-ish string_view. Features
/// are implemented as needed.

#include "nonstd/hash_bytes.h"
#include "nonstd/libc.h"
#include "perf.h"
#include "util/assert.h"
#include <algorithm>
#include <cstddef>
#include <memory>

namespace nonstd {

// fwd decls
class string_view;
constexpr bool operator==(const string_view &lhs, const string_view &rhs);
constexpr auto operator<=>(const string_view &lhs, const string_view &rhs);

/// \brief Nonstd version of std::string_view.
///
class string_view {
  class IterImpl;

public:
  using iterator = IterImpl;
  using reverse_iterator = std::reverse_iterator<IterImpl>;

  static constexpr size_t npos = -1;

  constexpr string_view() = default;
  constexpr string_view(const string_view &other) = default;
  constexpr string_view(const char *s) : _data(s), len(strlen(s)) {}
  constexpr string_view(const char *s, size_t len) : _data(s), len(len) {}
  template <std::contiguous_iterator InputIt>
  constexpr string_view(InputIt begin, InputIt end)
      : _data{std::to_address(begin)}, len(end - begin) {}
  string_view(std::nullptr_t) = delete;

  constexpr string_view &operator=(const string_view &rhs) = default;

  // iterators
  constexpr iterator begin() const { return iterator{_data}; }
  constexpr iterator end() const { return iterator{_data + len}; }
  constexpr reverse_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }
  constexpr reverse_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  // element access
  constexpr const char operator[](size_t pos) const { return _data[pos]; }
  constexpr const char at(size_t pos) const {
    ASSERT(pos < len);
    return _data[pos];
  }
  constexpr const char front() const { return _data[0]; }
  constexpr const char back() const { return _data[len - 1]; }
  constexpr const char *data() const { return _data; }

  // capacity
  constexpr size_t size() const { return len; }
  constexpr size_t length() const { return len; }
  constexpr bool empty() const { return len == 0; }

  // operations
  constexpr string_view substr(int32_t start, size_t count = npos) const {
    // This is different behavior than the standard, since this doesn't
    // throw if out of bounds. Instead, it silently crops to the
    // bounds. Clearly will also have unwanted behavior if `start` is a
    // negative signed int.
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
  constexpr size_t rfind(char needle) const {
    for (int i = size() - 1; i >= 0; --i) {
      if (data()[i] == needle) {
        return i;
      }
    }
    return npos;
  }
  constexpr bool contains(string_view needle) { return find(needle) != npos; }

private:
  class IterImpl {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = const char;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::contiguous_iterator_tag;

    // std::input_or_output_iterator
    constexpr value_type &operator*() const { return *ptr; }
    constexpr value_type *operator->() const { return ptr; }

    // std::forward_iterator
    constexpr IterImpl() = default;
    constexpr bool operator==(IterImpl other) const { return ptr == other.ptr; }
    constexpr IterImpl &operator++() {
      ++ptr;
      return *this;
    }
    constexpr IterImpl operator++(int) {
      IterImpl rv = *this;
      ++*this;
      return rv;
    }

    // std::bidirectional_iterator
    constexpr IterImpl &operator--() {
      --ptr;
      return *this;
    }
    constexpr IterImpl operator--(int) {
      IterImpl rv = *this;
      --*this;
      return rv;
    }

    // std::random_access_iterator
    constexpr std::strong_ordering operator<=>(IterImpl other) const {
      return ptr <=> other.ptr;
    }
    constexpr IterImpl &operator+=(difference_type n) {
      ptr += n;
      return *this;
    }
    constexpr friend IterImpl operator+(IterImpl i, difference_type n) {
      return i += n;
    }
    constexpr friend IterImpl operator+(difference_type n, IterImpl i) {
      return i += n;
    }
    constexpr IterImpl &operator-=(difference_type n) {
      ptr -= n;
      return *this;
    }
    constexpr friend IterImpl operator-(IterImpl i, difference_type n) {
      return i -= n;
    }
    constexpr difference_type operator-(IterImpl other) const {
      return ptr - other.ptr;
    }
    constexpr value_type &operator[](difference_type n) const { return ptr[n]; }

  private:
    friend string_view;
    explicit constexpr IterImpl(pointer _ptr) : ptr{_ptr} {}
    pointer ptr = nullptr;
  };

  const char *_data = nullptr;
  size_t len = 0;
};
static_assert(std::contiguous_iterator<string_view::iterator>);
static_assert(std::random_access_iterator<string_view::reverse_iterator>);

// non-member functions
constexpr bool operator==(const string_view &lhs, const string_view &rhs) {
  return lhs.length() == rhs.length() &&
         !strncmp(lhs.data(), rhs.data(), lhs.length());
}
constexpr auto operator<=>(const string_view &lhs, const string_view &rhs) {
  const size_t cmplen = std::min(lhs.length(), rhs.length());
  const auto res = nonstd::strncmp(lhs.data(), rhs.data(), cmplen);
  if (res != 0) {
    return res <=> 0;
  }
  return lhs.length() <=> rhs.length();
}

template <> struct hash<string_view> {
  size_t operator()(const string_view &key) {
    return hash_bytes(key.data(), key.size());
  }
};
template <> struct hash<const char *> {
  size_t operator()(const char *&key) { return hash_bytes(key, strlen(key)); }
};
template <size_t N> struct hash<const char[N]> {
  size_t operator()(const char (&key)[N]) { return hash_bytes(key, N - 1); }
};

// literals
constexpr nonstd::string_view operator""_sv(const char *s, size_t len) {
  return {s, len};
}

} // namespace nonstd
