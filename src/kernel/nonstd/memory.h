#pragma once

#include "nonstd/libc.h"
#include "util/objutil.h"
namespace nonstd {

// Mostly copied from
// https://andreasfertig.com/blog/2024/07/understanding-the-inner-workings-of-cpp-smart-pointers-the-unique_ptr/
template <typename T> class unique_ptr {
public:
  unique_ptr() = default;
  explicit unique_ptr(T *_ptr) : ptr{_ptr} {}

  NON_COPYABLE(unique_ptr);

  explicit unique_ptr(unique_ptr &&src) : ptr{src.ptr} { src.ptr = nullptr; }
  unique_ptr &operator=(unique_ptr &&src) {
    delete ptr;
    ptr = src.ptr;
    src.ptr = nullptr;
    return *this;
  }

  ~unique_ptr() {
    static_assert(sizeof(T) >= 0, "cannot delete an incomplete type");
    delete ptr;
  }

  T &operator*() const { return *ptr; }
  T *operator->() const { return get(); }
  T *get() const { return ptr; }

private:
  T *ptr;
};

} // namespace nonstd
