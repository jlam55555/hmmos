#pragma once

#include "nonstd/allocator.h"
#include "nonstd/hash_bytes.h"
#include "nonstd/string_view.h"
#include <cstddef>

namespace nonstd {

class string {
  using Allocator = Mallocator<char>;
  template <typename U> class IterImpl;

public:
  using iterator = IterImpl<char>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_iterator = IterImpl<const char>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static constexpr size_t npos = -1;

  string() = default;
  string(size_t count, char ch);
  template <std::input_iterator InputIt> string(InputIt first, InputIt last);
  string(const char *);
  string(const char *, size_t count);
  string(std::nullptr_t) = delete;
  explicit string(string_view);
  string(const string &);
  string(string &&) noexcept;

  ~string();

  string &operator=(const string &);
  string &operator=(string &&) noexcept;
  string &operator=(const char *);
  string &operator=(string_view);
  string &operator=(std::nullptr_t) = delete;

  // element access
  char &at(size_t pos);
  const char &at(size_t pos) const;
  char &operator[](size_t pos) { return _data[pos]; }
  const char &operator[](size_t pos) const { return _data[pos]; }
  char &front() { return _data[0]; }
  const char &front() const { return _data[0]; }
  char &back() { return _data[len - 1]; }
  const char &back() const { return _data[len - 1]; }
  const char *data() const { return _data; }
  char *data() { return _data; }
  const char *c_str() const { return _data; }
  char *c_str() { return _data; }
  operator string_view() const;

  // iterators
  iterator begin() { return iterator{_data}; }
  const_iterator begin() const { return const_iterator{_data}; }
  iterator end() { return iterator{_data + len}; }
  const_iterator end() const { return const_iterator{_data + len}; }
  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  const_reverse_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
  const_reverse_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  // capacity
  bool empty() const { return len == 0; }
  size_t size() const { return len; }
  size_t length() const { return len; }
  void reserve(size_t new_capacity);
  size_t capacity() const { return _capacity; }

  // modifiers
  void clear() { resize(0); }
  void push_back(char c);
  string &append(size_t count, char c);
  string &append(string_view sv);
  template <std::input_iterator InputIt>
  string &append(InputIt first, InputIt last);
  string &append(std::initializer_list<char> ilist);
  void resize(size_t count);
  void swap(string &other);

private:
  template <typename U> class IterImpl {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = U;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::contiguous_iterator_tag;

    // std::input_or_output_iterator
    value_type &operator*() const { return *ptr; }
    value_type *operator->() const { return ptr; }

    // std::forward_iterator
    IterImpl() = default;
    bool operator==(IterImpl other) const { return ptr == other.ptr; }
    IterImpl &operator++() {
      ++ptr;
      return *this;
    }
    IterImpl operator++(int) {
      IterImpl rv = *this;
      ++*this;
      return rv;
    }

    // std::bidirectional_iterator
    IterImpl &operator--() {
      --ptr;
      return *this;
    }
    IterImpl operator--(int) {
      IterImpl rv = *this;
      --*this;
      return rv;
    }

    // std::random_access_iterator
    std::strong_ordering operator<=>(IterImpl other) const {
      return ptr <=> other.ptr;
    }
    IterImpl &operator+=(difference_type n) {
      ptr += n;
      return *this;
    }
    friend IterImpl operator+(IterImpl i, difference_type n) { return i += n; }
    friend IterImpl operator+(difference_type n, IterImpl i) { return i += n; }
    IterImpl &operator-=(difference_type n) {
      ptr -= n;
      return *this;
    }
    friend IterImpl operator-(IterImpl i, difference_type n) { return i -= n; }
    difference_type operator-(IterImpl other) const { return ptr - other.ptr; }
    value_type &operator[](difference_type n) const { return ptr[n]; }

  private:
    friend string;
    explicit IterImpl(pointer _ptr) : ptr{_ptr} {}
    pointer ptr = nullptr;
  };

  size_t len = 0;
  size_t _capacity = 0;
  char *_data = nullptr;
};

static_assert(std::contiguous_iterator<string::iterator>);
static_assert(std::contiguous_iterator<string::const_iterator>);
static_assert(std::random_access_iterator<string::reverse_iterator>);
static_assert(std::random_access_iterator<string::const_reverse_iterator>);

// non-member functions
string operator+(const string &lhs, const string &rhs);
string operator+(const string &lhs, const char *rhs);
string operator+(const string &lhs, char rhs);
string operator+(const string &lhs, string_view rhs);
string operator+(const char *lhs, const string &rhs);
string operator+(char lhs, const string &rhs);
string operator+(string_view lhs, const string &rhs);

bool operator==(const string &lhs, const string &rhs);
bool operator==(const string &lhs, const char *rhs);
bool operator==(const char *lhs, const string &rhs);

template <> struct hash<string> {
  size_t operator()(const string &key) {
    return hash_bytes(key.data(), key.size());
  }
};

// template function definitions.
template <std::input_iterator InputIt>
string::string(InputIt first, InputIt last) {
  while (first != last) {
    push_back(*first++);
  }
}

template <std::input_iterator InputIt>
string &string::append(InputIt first, InputIt last) {
  while (first != last) {
    push_back(*first++);
  }
  return *this;
}

} // namespace nonstd
