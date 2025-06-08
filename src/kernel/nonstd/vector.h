#pragma once

#include "nonstd/allocator.h"
#include "util/assert.h"
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace nonstd {

template <typename T> class vector {
  using Allocator = Mallocator<T>;
  template <typename U> class IterImpl;

public:
  using iterator = IterImpl<T>;
  using const_iterator = IterImpl<const T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  vector() = default;
  vector(size_t count)
      : len{count}, _capacity{len}, _data{Allocator{}.allocate(_capacity)} {
    for (size_t i = 0; i < len; ++i) {
      ::new (&_data[i]) T{};
    }
  }
  vector(size_t count, const T &value)
      : len{count}, _capacity{len}, _data{Allocator{}.allocate(_capacity)} {
    for (size_t i = 0; i < len; ++i) {
      ::new (&_data[i]) T(value);
    }
  }
  template <std::input_iterator InputIt> vector(InputIt first, InputIt last) {
    while (first != last) {
      emplace_back(*first++);
    }
  }

  vector(const vector &v)
      : len{v.len}, _capacity{v._capacity}, _data{Allocator{}.allocate(
                                                _capacity)} {
    for (size_t i = 0; i < len; ++i) {
      ::new (&_data[i]) T(v[i]);
    }
  }
  vector(vector &&v) noexcept { *this = std::move(v); }
  vector(std::initializer_list<T> v)
      : len{v.size()}, _capacity{len}, _data{Allocator{}.allocate(_capacity)} {
    T *it = _data;
    for (auto &elem : v) {
      ::new (it++) T(std::move(elem));
    }
  }

  ~vector() {
    if (_capacity != 0) {
      for (size_t i = 0; i < len; ++i) {
        _data[i].~T();
      }
      Allocator{}.deallocate(_data, _capacity);
    }
  }

  vector &operator=(const vector &v) {
    *this = vector(v);
    return *this;
  }
  vector &operator=(vector &&v) {
    swap(v);
    return *this;
  }

  // element access
  T &at(size_t pos) {
    ASSERT(pos < size());
    return _data[pos];
  }
  const T &at(size_t pos) const {
    ASSERT(pos < size());
    return _data[pos];
  }
  T &operator[](size_t pos) { return _data[pos]; }
  const T &operator[](size_t pos) const { return _data[pos]; }
  T &front() { return _data[0]; }
  const T &front() const { return _data[0]; }
  T &back() { return _data[size() - 1]; }
  const T &back() const { return _data[size() - 1]; }
  T *data() { return _data; }
  const T *data() const { return _data; }

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
  bool empty() const { return size() == 0; }
  size_t size() const { return len; }
  void reserve(size_t new_capacity);
  size_t capacity() const { return _capacity; }

  // modifiers
  void clear() { resize(0); }
  void push_back(const T &v) { emplace_back(v); }
  void push_back(T &&v) { emplace_back(std::move(v)); }
  template <typename... Args> void emplace_back(Args &&...args) {
    reserve(len + 1);
    ::new (&_data[len++]) T(std::forward<Args>(args)...);
  }
  void pop_back() {
    ASSERT(len > 0);
    --len;
    _data[len].~T();
  }
  void resize(size_t count) {
    reserve(count);
    if (count > len) {
      for (size_t i = len; i < count; ++i) {
        ::new (&_data[i]) T{};
      }
    } else {
      for (size_t i = count; i < len; ++i) {
        _data[i].~T();
      }
    }
    len = count;
  }
  void swap(vector &v) {
    std::swap(len, v.len);
    std::swap(_capacity, v._capacity);
    std::swap(_data, v._data);
  }

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
    auto operator<=>(IterImpl other) const { return ptr <=> other.ptr; }
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
    friend vector;
    explicit IterImpl(pointer _ptr) : ptr{_ptr} {}
    pointer ptr = nullptr;
  };
  static_assert(std::contiguous_iterator<iterator>);
  static_assert(std::contiguous_iterator<const_iterator>);
  static_assert(std::random_access_iterator<reverse_iterator>);
  static_assert(std::random_access_iterator<const_reverse_iterator>);

  size_t len = 0;
  size_t _capacity = 0;
  T *_data = nullptr;
};

// non-member functions
template <typename T>
inline bool operator==(const vector<T> &lhs, const vector<T> &rhs);

// template function definitions
template <typename T> void vector<T>::reserve(size_t new_capacity) {
  if (new_capacity <= _capacity) {
    // No effect.
    return;
  }

  // We need to resize -- resize to at least double the current
  // capacity for O(1) amortized insertion.
  new_capacity = std::max(new_capacity, _capacity * 2);
  T *const old_data = _data;
  _data = Allocator{}.allocate(new_capacity);
  for (size_t i = 0; i < len; ++i) {
    ::new (&_data[i]) T(std::move(old_data[i]));
    old_data[i].~T();
  }
  if (_capacity != 0) {
    Allocator{}.deallocate(old_data, _capacity);
  }
  _capacity = new_capacity;
}

template <typename T>
bool operator==(const vector<T> &lhs, const vector<T> &rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

} // namespace nonstd
