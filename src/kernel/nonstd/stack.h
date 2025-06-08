#pragma once

#include "nonstd/deque.h"

namespace nonstd {

template <typename T, typename Container = deque<T>> class stack {
public:
  explicit stack() = default;
  explicit stack(const Container &_cont) : cont{_cont} {}
  explicit stack(Container &&_cont) : cont{std::move(_cont)} {}
  stack(const stack &other) = default;
  stack(stack &&other) = default;
  template <std::input_iterator InputIt>
  stack(InputIt first, InputIt last) : cont(first, last) {}

  ~stack() = default;

  stack &operator=(const stack &other) = default;
  stack &operator=(stack &&other) = default;

  // element access
  T &top() { return cont.back(); }
  const T &top() const { return cont.back(); }

  // capacity
  bool empty() const { return cont.empty(); }
  size_t size() const { return cont.size(); }

  // modifiers
  void push(const T &value) { cont.push_back(value); }
  void push(T &&value) { cont.push_back(std::move(value)); }
  template <typename... Args> void emplace(Args &&...args) {
    cont.emplace_back(std::forward<Args>(args)...);
  }
  void pop() { cont.pop_back(); }

  // non-member functions
  friend bool operator==(const stack &lhs, const stack &rhs) {
    return lhs.cont == rhs.cont;
  }

private:
  Container cont;
};

} // namespace nonstd
